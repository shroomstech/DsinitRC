#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main() {
    // 1. Virtuelle Dateisysteme einhängen
    if (mount("proc", "/proc", "proc", 0, NULL) != 0) {
        perror("dsinit: mount /proc failed");
    }
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) != 0) {
        perror("dsinit: mount /sys failed");
    }
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) != 0) {
        perror("dsinit: mount /dev failed");
    }

    // 2. Hostnamen setzen
    if (sethostname("cursedos", 8) != 0) {
        perror("dsinit: sethostname failed");
    }

    // 3. Grundlegende Umgebungsvariablen setzen
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);
    setenv("USER", "livecd", 1);
    setenv("LOGNAME", "livecd", 1);
    setenv("HOME", "/root", 1);
    setenv("TERM", "linux", 1);

    printf("\n");
    printf("========================================\n");
    printf("   CursedOS dsinit (PID 1) started\n");
    printf("========================================\n");
    printf("\n");

    // 4. Shell-Loop als PID 1 (hält das System am Leben und fängt beendete Prozesse ab)
    while (1) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("dsinit: fork failed");
            sleep(1);
            continue;
        }

        if (pid == 0) {
            // Kindprozess: Ausführen der Shell
            // Wir nutzen ein einfaches Terminal-Setup
            execl("/bin/sh", "sh", NULL);
            // Falls execl fehlschlägt:
            perror("dsinit: execl failed");
            _exit(1);
        } else {
            // Elternprozess: Auf das Ende der Shell warten (verhindert Zombies)
            int status;
            pid_t waited_pid;
            do {
                waited_pid = waitpid(pid, &status, 0);
            } while (waited_pid == -1);

            printf("\n[dsinit] Shell exited. Restarting in 1 second...\n\n");
            sleep(1);
        }
    }

    return 0;
}
