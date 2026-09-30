#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#define CONFIG_FILE "/etc/dsinitrc.conf"
#define DEFAULT_HOSTNAME "local-lts"

// Konfiguration sicher einlesen mit Fallbacks
void parse_config() {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        printf("[dsinitrc-LTS] Notice: Config %s not found. Applying LTS defaults.\n", CONFIG_FILE);
        sethostname(DEFAULT_HOSTNAME, strlen(DEFAULT_HOSTNAME));
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Kommentare und Leerzeilen überspringen
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        // Zeile trennen
        char *key = strtok(line, "=\r\n");
        char *value = strtok(NULL, "=\r\n");

        if (key && value) {
            if (strcmp(key, "HOSTNAME") == 0) {
                sethostname(value, strlen(value));
                printf("[dsinitrc-LTS] Hostname configured: %s\n", value);
            } else if (strcmp(key, "TERM") == 0) {
                setenv("TERM", value, 1);
            } else if (strcmp(key, "PS1") == 0) {
                setenv("PS1", value, 1);
            }
        }
    }
    fclose(file);
}

// Virtuelle Dateisysteme sicher einhängen
void mount_virtual_filesystems() {
    struct {
        const char *source;
        const char *target;
        const char *filesystem;
    } mounts[] = {
        {"proc", "/proc", "proc"},
        {"sysfs", "/sys", "sysfs"},
        {"devtmpfs", "/dev", "devtmpfs"}
    };

    for (int i = 0; i < 3; i++) {
        if (mount(mounts[i].source, mounts[i].target, mounts[i].filesystem, 0, NULL) != 0) {
            // Wenn es bereits eingehängt ist, ignorieren wir den Fehler (EBUSY)
            if (errno != EBUSY) {
                fprintf(stderr, "[dsinitrc-LTS] Warning: Failed to mount %s (%s)\n", mounts[i].target, strerror(errno));
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // 1. CLI Argumente prüfen (LTS-Befehle)
    if (argc > 1) {
        if (strcmp(argv[1], "--reboot") == 0) {
            printf("[dsinitrc-LTS] System reboot initiated...\n");
            sync();
            reboot(RB_AUTOBOOT);
            return 0;
        }
        else if (strcmp(argv[1], "--help") == 0) {
            printf("dsinitrc LTS (Long Term Support) Init System\n");
            printf("  --reboot    Reboots the system safely\n");
            printf("  --help      Displays this help screen\n");
            printf("Config path: %s\n", CONFIG_FILE);
            return 0;
        }
    }

    // PID 1 Check zur Sicherheit
    if (getpid() != 1) {
        fprintf(stderr, "[dsinitrc-LTS] Warning: Not running as PID 1! Behaviour might be unexpected.\n");
    }

    // 2. Basis-Umgebung initialisieren
    setenv("PATH", "/bin:/usr/bin:/sbin:/usr/sbin", 1);
    setenv("TERM", "linux", 1);
    setenv("PS1", "\\u@\\h:\\w\\$ ", 1);

    // 3. Mounts & Config laden
    mount_virtual_filesystems();
    parse_config();

    // 4. Start-Banner
    printf("\033[H\033[J");
    printf("========================================\n");
    printf("  dsinitrc - LTS Core Environment      \n");
    printf("  Status: Stable / Long-Term Support    \n");
    printf("========================================\n\n");

    // 5. Haupt-Loop (Stabile Service-/Shell-Ausführung)
    while (1) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("[dsinitrc-LTS] Fork failed");
            sleep(2);
            continue;
        }

        if (pid == 0) {
            // Kindprozess: Startet die Standard-Shell
            execl("/bin/sh", "sh", NULL);
            perror("[dsinitrc-LTS] Failed to execute shell");
            _exit(1);
        } else {
            int status;
            pid_t waited;

            // Warten auf den Kindprozess, Zombies verhindern
            do {
                waited = waitpid(pid, &status, 0);
            } while (waited == -1 && errno == EINTR);

            printf("\n[dsinitrc-LTS] Session terminated. Respawning in 1 second...\n");
            sleep(1);
        }
    }

    return 0;
}
