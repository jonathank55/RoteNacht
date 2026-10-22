/*
 * RoteNacht - Automatischer macOS-Nachtmodus (Schwarz/Rot & Binärfarbig)
 * https://github.com/jonathank55/RoteNacht
 *
 * Lizenz: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

#define VERSION "1.1.0"
#define DAEMON_LABEL "com.rotenacht.daemon"

#define CAT_COLOR 1
#define TYPE_TINT 16

// Private MediaAccessibility Funktionen für luminanzbasiertes Color Tinting
extern int    MADisplayFilterPrefGetCategoryEnabled(long category);
extern void   MADisplayFilterPrefSetCategoryEnabled(long category, int on);
extern int    MADisplayFilterPrefGetType(long category);
extern void   MADisplayFilterPrefSetType(long category, int type);
extern double MADisplayFilterPrefGetSingleColorHue(long category);
extern void   MADisplayFilterPrefSetSingleColorHue(long category, double hue);
extern double MADisplayFilterPrefGetSingleColorIntensity(long category);
extern void   MADisplayFilterPrefSetSingleColorIntensity(long category, double v);

typedef struct {
    char start_time[16];   // z. B. "20:00"
    char end_time[16];     // z. B. "06:00"
    bool enabled;          // Automatik aktiv
    bool binary_mode;      // true = harter Schwellenwert (1-Bit), false = weicher Rot-Farbverlauf
    int manual_override;   // 0 = Auto, 1 = manuell EIN, -1 = manuell AUS
} Config;

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_reload_config = 0;
static bool g_current_is_red = false;
static io_connect_t g_power_port = 0;
static io_object_t g_power_notifier = 0;

static void get_home_dir(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, size, "%s", home);
    } else {
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            snprintf(buf, size, "%s", pw->pw_dir);
        } else {
            snprintf(buf, size, "/tmp");
        }
    }
}

static void get_config_dir(char *buf, size_t size) {
    char home[1024];
    get_home_dir(home, sizeof(home));
    snprintf(buf, size, "%s/.config/rotenacht", home);
}

static void get_config_file(char *buf, size_t size) {
    char dir[1024];
    get_config_dir(dir, sizeof(dir));
    snprintf(buf, size, "%s/config.json", dir);
}

static void get_pid_file(char *buf, size_t size) {
    char dir[1024];
    get_config_dir(dir, sizeof(dir));
    snprintf(buf, size, "%s/rotenacht.pid", dir);
}

static void get_plist_path(char *buf, size_t size) {
    char home[1024];
    get_home_dir(home, sizeof(home));
    snprintf(buf, size, "%s/Library/LaunchAgents/%s.plist", home, DAEMON_LABEL);
}

static void get_executable_path(char *buf, size_t size) {
    char home[1024];
    get_home_dir(home, sizeof(home));
    snprintf(buf, size, "%s/bin/nachtrot", home);
    if (access(buf, X_OK) != 0) {
        uint32_t bufsize = (uint32_t)size;
        extern int _NSGetExecutablePath(char* buf, uint32_t* bufsize);
        if (_NSGetExecutablePath(buf, &bufsize) != 0) {
            snprintf(buf, size, "/usr/local/bin/nachtrot");
        }
    }
}

static void ensure_config_dir(void) {
    char dir[1024];
    get_config_dir(dir, sizeof(dir));
    mkdir(dir, 0755);
}

// Konfiguration laden
static void load_config(Config *cfg) {
    strncpy(cfg->start_time, "20:00", sizeof(cfg->start_time));
    strncpy(cfg->end_time, "06:00", sizeof(cfg->end_time));
    cfg->enabled = true;
    cfg->binary_mode = false;
    cfg->manual_override = 0;

    char file[1024];
    get_config_file(file, sizeof(file));
    FILE *f = fopen(file, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[64];
        if (sscanf(line, " \"%[^\"]\" : \"%[^\"]\"", key, val) == 2 ||
            sscanf(line, " \"%[^\"]\" : %[^, \n\r]", key, val) == 2 ||
            sscanf(line, "\"%[^\"]\": \"%[^\"]\"", key, val) == 2 ||
            sscanf(line, "\"%[^\"]\": %[^, \n\r]", key, val) == 2) {
            
            if (strcmp(key, "start_time") == 0) {
                strncpy(cfg->start_time, val, sizeof(cfg->start_time) - 1);
            } else if (strcmp(key, "end_time") == 0) {
                strncpy(cfg->end_time, val, sizeof(cfg->end_time) - 1);
            } else if (strcmp(key, "enabled") == 0) {
                cfg->enabled = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
            } else if (strcmp(key, "binary_mode") == 0) {
                cfg->binary_mode = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
            } else if (strcmp(key, "manual_override") == 0) {
                cfg->manual_override = atoi(val);
            }
        }
    }
    fclose(f);
}

// Konfiguration speichern
static void save_config(const Config *cfg) {
    ensure_config_dir();
    char file[1024];
    get_config_file(file, sizeof(file));

    FILE *f = fopen(file, "w");
    if (!f) {
        fprintf(stderr, "Fehler beim Schreiben der Konfiguration nach %s\n", file);
        return;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"start_time\": \"%s\",\n", cfg->start_time);
    fprintf(f, "  \"end_time\": \"%s\",\n", cfg->end_time);
    fprintf(f, "  \"enabled\": %s,\n", cfg->enabled ? "true" : "false");
    fprintf(f, "  \"binary_mode\": %s,\n", cfg->binary_mode ? "true" : "false");
    fprintf(f, "  \"manual_override\": %d\n", cfg->manual_override);
    fprintf(f, "}\n");
    fclose(f);
}

// Zeit-String parsen ("20:00", "20.00", "20")
static bool parse_time(const char *str, int *hours, int *minutes) {
    int h = -1, m = -1;
    if (sscanf(str, "%d:%d", &h, &m) == 2 || sscanf(str, "%d.%d", &h, &m) == 2) {
        if (h >= 0 && h < 24 && m >= 0 && m < 60) {
            *hours = h;
            *minutes = m;
            return true;
        }
    } else if (sscanf(str, "%d", &h) == 1) {
        if (h >= 0 && h < 24) {
            *hours = h;
            *minutes = 0;
            return true;
        }
    }
    return false;
}

// Prüfen, ob die aktuelle Zeit im Nachtfenster liegt
static bool is_current_time_in_range(const char *start_str, const char *end_str) {
    int sh = 20, sm = 0, eh = 6, em = 0;
    if (!parse_time(start_str, &sh, &sm)) { sh = 20; sm = 0; }
    if (!parse_time(end_str, &eh, &em)) { eh = 6; em = 0; }

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    int cur_min = tm_now->tm_hour * 60 + tm_now->tm_min;
    int start_min = sh * 60 + sm;
    int end_min = eh * 60 + em;

    if (start_min == end_min) {
        return false;
    }
    if (start_min > end_min) {
        return (cur_min >= start_min || cur_min < end_min);
    } else {
        return (cur_min >= start_min && cur_min < end_min);
    }
}

// Prüfen, ob der Rotmodus aktuell im Display aktiv ist
static bool is_display_red_active(void) {
    if (MADisplayFilterPrefGetCategoryEnabled(CAT_COLOR)) {
        return true;
    }
    CGDirectDisplayID mainDisplay = CGMainDisplayID();
    CGGammaValue r[256], g[256], b[256];
    uint32_t count = 0;
    if (CGGetDisplayTransferByTable(mainDisplay, 256, r, g, b, &count) == kCGErrorSuccess && count >= 128) {
        return (g[128] < 0.05f && b[128] < 0.05f && r[128] > 0.1f);
    }
    return false;
}

// Rote Gamma-Tabelle auf alle aktiven Displays anwenden (Reines Schwarz & Rot)
static void apply_red_gamma(bool binary_mode) {
    uint32_t maxDisplays = 16;
    CGDirectDisplayID displays[16];
    uint32_t displayCount = 0;
    CGGetActiveDisplayList(maxDisplays, displays, &displayCount);

    CGGammaValue red[256];
    CGGammaValue zero[256];

    for (uint32_t i = 0; i < 256; i++) {
        zero[i] = 0.0f;
        if (binary_mode) {
            red[i] = (i >= 128) ? 1.0f : 0.0f;
        } else {
            red[i] = (CGGammaValue)i / 255.0f;
        }
    }

    for (uint32_t d = 0; d < displayCount; d++) {
        CGSetDisplayTransferByTable(displays[d], 256, red, zero, zero);
    }
}

// RoteNacht aktivieren: Native Luminanz-Farbanpassung + Hardware-Filter
static void enable_rotenacht(bool binary_mode) {
    // Stufe 1: Native macOS Color Tint (rechnet Grün & Blau gemäß Augenempfindlichkeit in Rotlicht um)
    MADisplayFilterPrefSetType(CAT_COLOR, TYPE_TINT);
    MADisplayFilterPrefSetSingleColorHue(CAT_COLOR, 0.0);
    MADisplayFilterPrefSetSingleColorIntensity(CAT_COLOR, 1.0);
    MADisplayFilterPrefSetCategoryEnabled(CAT_COLOR, 1);

    // Stufe 2: Hardware-Gamma-Ramp (garantiert 0% Restemission bei Grün und Blau)
    apply_red_gamma(binary_mode);
    g_current_is_red = true;
}

// RoteNacht deaktivieren & Normalfarben wiederherstellen
static void disable_rotenacht(void) {
    MADisplayFilterPrefSetCategoryEnabled(CAT_COLOR, 0);
    CGDisplayRestoreColorSyncSettings();
    g_current_is_red = false;
}

// PID-Verwaltung für den Hintergrunddienst
static pid_t read_daemon_pid(void) {
    char pfile[1024];
    get_pid_file(pfile, sizeof(pfile));
    FILE *f = fopen(pfile, "r");
    if (!f) return -1;
    pid_t pid = -1;
    if (fscanf(f, "%d", &pid) != 1) pid = -1;
    fclose(f);
    if (pid > 0 && kill(pid, 0) == 0) {
        return pid;
    }
    return -1;
}

static void write_daemon_pid(pid_t pid) {
    ensure_config_dir();
    char pfile[1024];
    get_pid_file(pfile, sizeof(pfile));
    FILE *f = fopen(pfile, "w");
    if (f) {
        fprintf(f, "%d\n", pid);
        fclose(f);
    }
}

static void remove_daemon_pid(void) {
    char pfile[1024];
    get_pid_file(pfile, sizeof(pfile));
    unlink(pfile);
}

static void notify_daemon_reload(void) {
    pid_t pid = read_daemon_pid();
    if (pid > 0) {
        kill(pid, SIGHUP);
    }
}

// Signale & Callbacks für Daemon
static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        g_running = 0;
    } else if (sig == SIGHUP) {
        g_reload_config = 1;
    }
}

static void on_display_reconfigure(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo) {
    (void)display;
    (void)userInfo;
    if (flags & kCGDisplayBeginConfigurationFlag) return;
    if (g_current_is_red) {
        Config cfg;
        load_config(&cfg);
        enable_rotenacht(cfg.binary_mode);
    }
}

static void power_callback(void *refCon, io_service_t service, natural_t messageType, void *messageArgument) {
    (void)refCon;
    (void)service;
    if (messageType == kIOMessageSystemWillRestart || messageType == kIOMessageSystemWillPowerOff) {
        disable_rotenacht();
    } else if (messageType == kIOMessageSystemHasPoweredOn) {
        if (g_current_is_red) {
            Config cfg;
            load_config(&cfg);
            enable_rotenacht(cfg.binary_mode);
        }
    }
    if (messageType == kIOMessageCanSystemSleep || messageType == kIOMessageSystemWillSleep) {
        IOAllowPowerChange(g_power_port, (long)messageArgument);
    }
}

// Daemon-Schleife
static int run_daemon(void) {
    write_daemon_pid(getpid());

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGHUP, handle_signal);

    CGDisplayRegisterReconfigurationCallback(on_display_reconfigure, NULL);

    IONotificationPortRef notifyPortRef;
    g_power_port = IORegisterForSystemPower(NULL, &notifyPortRef, power_callback, &g_power_notifier);
    if (notifyPortRef) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(),
                           IONotificationPortGetRunLoopSource(notifyPortRef),
                           kCFRunLoopDefaultMode);
    }

    Config cfg;
    load_config(&cfg);

    int last_check_minute = -1;

    while (g_running) {
        if (g_reload_config) {
            g_reload_config = 0;
            load_config(&cfg);
        }

        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        int cur_minute = tm_now->tm_hour * 60 + tm_now->tm_min;

        bool should_be_red = false;

        if (cfg.manual_override == 1) {
            should_be_red = true;
        } else if (cfg.manual_override == -1) {
            should_be_red = false;
        } else if (cfg.enabled) {
            should_be_red = is_current_time_in_range(cfg.start_time, cfg.end_time);
        }

        if (last_check_minute != -1 && cur_minute != last_check_minute) {
            bool natural_night = is_current_time_in_range(cfg.start_time, cfg.end_time);
            int sh, sm, eh, em;
            parse_time(cfg.start_time, &sh, &sm);
            parse_time(cfg.end_time, &eh, &em);
            int start_min = sh * 60 + sm;
            int end_min = eh * 60 + em;

            if (cur_minute == start_min || cur_minute == end_min) {
                if (cfg.manual_override != 0) {
                    cfg.manual_override = 0;
                    save_config(&cfg);
                    should_be_red = natural_night;
                }
            }
        }
        last_check_minute = cur_minute;

        if (should_be_red) {
            enable_rotenacht(cfg.binary_mode);
        } else {
            if (g_current_is_red) {
                disable_rotenacht();
            }
        }

        for (int i = 0; i < 2 && g_running && !g_reload_config; i++) {
            sleep(1);
        }
    }

    disable_rotenacht();
    remove_daemon_pid();

    if (g_power_port && g_power_notifier) {
        IODeregisterForSystemPower(&g_power_notifier);
    }
    CGDisplayRemoveReconfigurationCallback(on_display_reconfigure, NULL);

    return 0;
}

// Launchd Agent installieren & starten
static bool install_launchd_service(void) {
    char plist_path[1024];
    get_plist_path(plist_path, sizeof(plist_path));

    char exec_path[1024];
    get_executable_path(exec_path, sizeof(exec_path));

    char home[1024];
    get_home_dir(home, sizeof(home));

    char agent_dir[1024];
    snprintf(agent_dir, sizeof(agent_dir), "%s/Library/LaunchAgents", home);
    mkdir(agent_dir, 0755);

    FILE *f = fopen(plist_path, "w");
    if (!f) {
        fprintf(stderr, "Fehler: Konnte %s nicht erstellen.\n", plist_path);
        return false;
    }

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
    fprintf(f, "<plist version=\"1.0\">\n");
    fprintf(f, "<dict>\n");
    fprintf(f, "    <key>Label</key>\n");
    fprintf(f, "    <string>%s</string>\n", DAEMON_LABEL);
    fprintf(f, "    <key>ProgramArguments</key>\n");
    fprintf(f, "    <array>\n");
    fprintf(f, "        <string>%s</string>\n", exec_path);
    fprintf(f, "        <string>daemon</string>\n");
    fprintf(f, "    </array>\n");
    fprintf(f, "    <key>RunAtLoad</key>\n");
    fprintf(f, "    <true/>\n");
    fprintf(f, "    <key>KeepAlive</key>\n");
    fprintf(f, "    <true/>\n");
    fprintf(f, "    <key>StandardOutPath</key>\n");
    fprintf(f, "    <string>%s/.config/rotenacht/rotenacht.log</string>\n", home);
    fprintf(f, "    <key>StandardErrorPath</key>\n");
    fprintf(f, "    <string>%s/.config/rotenacht/rotenacht.err</string>\n", home);
    fprintf(f, "</dict>\n");
    fprintf(f, "</plist>\n");
    fclose(f);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "launchctl unload \"%s\" 2>/dev/null; launchctl load \"%s\"", plist_path, plist_path);
    system(cmd);

    return true;
}

// Launchd Agent stoppen & entfernen
static bool uninstall_launchd_service(void) {
    char plist_path[1024];
    get_plist_path(plist_path, sizeof(plist_path));

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "launchctl unload \"%s\" 2>/dev/null", plist_path);
    system(cmd);

    unlink(plist_path);

    pid_t pid = read_daemon_pid();
    if (pid > 0) {
        kill(pid, SIGTERM);
        sleep(1);
    }
    remove_daemon_pid();
    disable_rotenacht();

    return true;
}

// Hilfe-Anzeige
static void print_help(void) {
    printf("\033[1;31mRoteNacht\033[0m v%s - Automatischer Schwarz/Rot-Monochrom-Nachtmodus\n\n", VERSION);
    printf("\033[1mVERWENDUNG:\033[0m\n");
    printf("  nachtrot                      Status anzeigen & Kurzanleitung\n");
    printf("  nachtrot \033[1;33m-z <Uhrzeit>\033[0m         Startzeit für Rotmodus setzen (z. B. 20:00)\n");
    printf("  nachtrot -e <Uhrzeit>         Endzeit für Rotmodus setzen (Standard: 06:00)\n");
    printf("  nachtrot \033[1;32mein\033[0m                  Rotmodus sofort manuell einschalten\n");
    printf("  nachtrot \033[1;31maus\033[0m                  Rotmodus sofort manuell ausschalten\n");
    printf("  nachtrot toggle               Zwischen Rotmodus und Normalfarben umschalten\n");
    printf("  nachtrot auto                 Manuellen Override beenden, Zeitplan folgen\n");
    printf("  nachtrot -b | --binary        Binärmodus umschalten (weicher Farbverlauf vs. 1-Bit)\n");
    printf("  nachtrot status               Detaillierten System- & Dienststatus prüfen\n");
    printf("  nachtrot install              Hintergrunddienst (LaunchAgent) installieren & starten\n");
    printf("  nachtrot uninstall            Hintergrunddienst vollständig deinstallieren\n");
    printf("  nachtrot -h | --help          Diese Hilfe anzeigen\n\n");
    printf("\033[1mBEISPIELE:\033[0m\n");
    printf("  nachtrot -z 20:00             (Aktiviert Rotmodus täglich ab 20:00 Uhr)\n");
    printf("  nachtrot ein                  (Schaltet den Bildschirm sofort auf Schwarz/Rot)\n");
    printf("  nachtrot aus                  (Stellt die Standardfarben sofort wieder her)\n\n");
}

// Status-Anzeige
static void print_status(void) {
    Config cfg;
    load_config(&cfg);

    pid_t daemon_pid = read_daemon_pid();
    bool is_red = is_display_red_active();
    bool is_in_window = is_current_time_in_range(cfg.start_time, cfg.end_time);

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);

    printf("\n\033[1;31m══════════════════════════════════════════════════\033[0m\n");
    printf("\033[1;31m  🌙 RoteNacht - Status & Konfiguration\033[0m\n");
    printf("\033[1;31m══════════════════════════════════════════════════\033[0m\n\n");

    printf("  \033[1mAktuelle Uhrzeit:\033[0m     %02d:%02d:%02d\n", tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    printf("  \033[1mRot-Startzeit (-z):\033[0m   \033[1;33m%s Uhr\033[0m\n", cfg.start_time);
    printf("  \033[1mRot-Endzeit (-e):\033[0m     %s Uhr\n", cfg.end_time);
    printf("  \033[1mFarbraum-Modus:\033[0m       %s\n", cfg.binary_mode ? "1-Bit Binär (Hart)" : "Schwarz/Rot Luminanz-Monochrom (Optimiert)");
    printf("  \033[1mAutomatik-Plan:\033[0m       %s\n", cfg.enabled ? "\033[1;32mAktiviert\033[0m" : "\033[1;31mDeaktiviert\033[0m");

    if (cfg.manual_override == 1) {
        printf("  \033[1mManueller Modus:\033[0m      \033[1;33mManuell erzwungen: EIN\033[0m (Tipp: 'nachtrot auto')\n");
    } else if (cfg.manual_override == -1) {
        printf("  \033[1mManueller Modus:\033[0m      \033[1;33mManuell pausiert: AUS\033[0m (Tipp: 'nachtrot auto')\n");
    } else {
        printf("  \033[1mManueller Modus:\033[0m      Inaktiv (folgt Zeitplan)\n");
    }

    printf("  \033[1mAktuelles Zeitfenster:\033[0m %s\n", is_in_window ? "\033[1;31mNachtmodus aktiv\033[0m" : "\033[1;32mTagmodus aktiv\033[0m");
    printf("  \033[1mBildschirm-Filter:\033[0m    %s\n", is_red ? "\033[1;31m● ROT / MONOCHROM AKTIV\033[0m" : "\033[1;32m○ NORMALFARBEN\033[0m");
    printf("  \033[1mHintergrunddienst:\033[0m    %s", daemon_pid > 0 ? "\033[1;32mLäuft\033[0m (PID " : "\033[1;31mNicht aktiv\033[0m\n");
    if (daemon_pid > 0) printf("%d)\n", daemon_pid);

    printf("\n\033[1;31m──────────────────────────────────────────────────\033[0m\n");
    printf("  Befehle: 'nachtrot -z 20:00' | 'nachtrot ein' | 'nachtrot aus'\n");
    printf("\033[1;31m──────────────────────────────────────────────────\033[0m\n\n");
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        print_status();
        return 0;
    }

    if (strcmp(argv[1], "daemon") == 0) {
        return run_daemon();
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(argv[1], "status") == 0 || strcmp(argv[1], "-s") == 0) {
        print_status();
        return 0;
    }

    if (strcmp(argv[1], "install") == 0) {
        printf("Installiere RoteNacht Hintergrunddienst...\n");
        if (install_launchd_service()) {
            printf("\033[1;32m✓ Hintergrunddienst erfolgreich installiert und gestartet!\033[0m\n");
            print_status();
        }
        return 0;
    }

    if (strcmp(argv[1], "uninstall") == 0) {
        printf("Deinstalliere RoteNacht Hintergrunddienst...\n");
        if (uninstall_launchd_service()) {
            printf("\033[1;32m✓ Dienst gestoppt und LaunchAgent entfernt. Bildschirmfarben normalisiert.\033[0m\n");
        }
        return 0;
    }

    // Manuell einschalten: "ein", "an", "on", "start"
    if (strcmp(argv[1], "ein") == 0 || strcmp(argv[1], "an") == 0 ||
        strcmp(argv[1], "on") == 0 || strcmp(argv[1], "--ein") == 0 || strcmp(argv[1], "--an") == 0 || strcmp(argv[1], "start") == 0) {
        Config cfg;
        load_config(&cfg);
        cfg.manual_override = 1;
        save_config(&cfg);
        enable_rotenacht(cfg.binary_mode);
        notify_daemon_reload();
        printf("\033[1;31m● RoteNacht aktiviert:\033[0m Bildschirm ist jetzt dauerhaft rot/monochrom geschaltet (mit voller Grün-Klarheit).\n");
        printf("  (Tipp: 'nachtrot aus' zum Ausschalten, 'nachtrot auto' für Zeitplan)\n");
        return 0;
    }

    // Manuell ausschalten: "aus", "off", "stop"
    if (strcmp(argv[1], "aus") == 0 || strcmp(argv[1], "off") == 0 ||
        strcmp(argv[1], "--aus") == 0 || strcmp(argv[1], "--off") == 0 || strcmp(argv[1], "stop") == 0) {
        Config cfg;
        load_config(&cfg);
        cfg.manual_override = -1;
        save_config(&cfg);
        disable_rotenacht();
        notify_daemon_reload();
        printf("\033[1;32m○ RoteNacht deaktiviert:\033[0m Bildschirmfarben normalisiert.\n");
        printf("  (Tipp: 'nachtrot ein' zum Einschalten, 'nachtrot auto' für Zeitplan)\n");
        return 0;
    }

    // Toggle
    if (strcmp(argv[1], "toggle") == 0) {
        Config cfg;
        load_config(&cfg);
        if (is_display_red_active()) {
            cfg.manual_override = -1;
            save_config(&cfg);
            disable_rotenacht();
            printf("\033[1;32m○ RoteNacht deaktiviert.\033[0m\n");
        } else {
            cfg.manual_override = 1;
            save_config(&cfg);
            enable_rotenacht(cfg.binary_mode);
            printf("\033[1;31m● RoteNacht aktiviert.\033[0m\n");
        }
        notify_daemon_reload();
        return 0;
    }

    // Auto-Modus wiederherstellen
    if (strcmp(argv[1], "auto") == 0) {
        Config cfg;
        load_config(&cfg);
        cfg.manual_override = 0;
        save_config(&cfg);
        notify_daemon_reload();
        bool in_window = is_current_time_in_range(cfg.start_time, cfg.end_time);
        if (in_window) {
            enable_rotenacht(cfg.binary_mode);
            printf("\033[1;32m✓ Automatik wiederhergestellt.\033[0m Aktuell im Nachtfenster -> \033[1;31mRotmodus dauerhaft aktiv\033[0m.\n");
        } else {
            disable_rotenacht();
            printf("\033[1;32m✓ Automatik wiederhergestellt.\033[0m Aktuell im Tagfenster -> \033[1;32mNormalfarben aktiv\033[0m.\n");
        }
        return 0;
    }

    // Binärmodus umschalten
    if (strcmp(argv[1], "-b") == 0 || strcmp(argv[1], "--binary") == 0) {
        Config cfg;
        load_config(&cfg);
        cfg.binary_mode = !cfg.binary_mode;
        save_config(&cfg);
        notify_daemon_reload();
        printf("Farbraum geändert zu: \033[1m%s\033[0m\n",
               cfg.binary_mode ? "1-Bit Binär (Hart)" : "Schwarz/Rot Monochrom (Luminanz-optimiert)");
        if (is_display_red_active() || cfg.manual_override == 1 || (cfg.manual_override == 0 && is_current_time_in_range(cfg.start_time, cfg.end_time))) {
            enable_rotenacht(cfg.binary_mode);
        }
        return 0;
    }

    Config cfg;
    load_config(&cfg);
    bool changed = false;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-z") == 0 || strcmp(argv[i], "--zeit") == 0) && i + 1 < argc) {
            int h, m;
            if (parse_time(argv[i + 1], &h, &m)) {
                snprintf(cfg.start_time, sizeof(cfg.start_time), "%02d:%02d", h, m);
                changed = true;
                i++;
            } else {
                fprintf(stderr, "\033[1;31mFehler:\033[0m Ungültiges Zeitformat '%s'. Bitte 'HH:MM' (z. B. 20:00) verwenden.\n", argv[i + 1]);
                return 1;
            }
        } else if ((strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--ende") == 0) && i + 1 < argc) {
            int h, m;
            if (parse_time(argv[i + 1], &h, &m)) {
                snprintf(cfg.end_time, sizeof(cfg.end_time), "%02d:%02d", h, m);
                changed = true;
                i++;
            } else {
                fprintf(stderr, "\033[1;31mFehler:\033[0m Ungültiges Zeitformat '%s'. Bitte 'HH:MM' (z. B. 06:00) verwenden.\n", argv[i + 1]);
                return 1;
            }
        } else {
            fprintf(stderr, "\033[1;31mUnbekannter Parameter:\033[0m '%s'. 'nachtrot --help' für Hilfe.\n", argv[i]);
            return 1;
        }
    }

    if (changed) {
        save_config(&cfg);

        pid_t pid = read_daemon_pid();
        if (pid <= 0) {
            install_launchd_service();
        } else {
            notify_daemon_reload();
        }

        printf("\033[1;32m✓ Zeitplan aktualisiert:\033[0m Täglich ab \033[1;33m%s Uhr\033[0m bis \033[1;33m%s Uhr\033[0m.\n",
               cfg.start_time, cfg.end_time);

        if (cfg.manual_override == 0 && is_current_time_in_range(cfg.start_time, cfg.end_time)) {
            enable_rotenacht(cfg.binary_mode);
            printf("\033[1;31m● Aktuell im Nachtfenster -> Rotmodus sofort eingeschaltet.\033[0m\n");
        } else if (cfg.manual_override == 0) {
            disable_rotenacht();
            printf("\033[1;32m○ Aktuell im Tagfenster -> Bildschirm bleibt bis %s Uhr im Normalmodus.\033[0m\n", cfg.start_time);
        }
    }

    return 0;
}
