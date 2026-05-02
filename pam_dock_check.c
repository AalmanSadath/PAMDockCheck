/*
 * pam_dock_check.c
 *
 * PAM module for the Framework 13 laptop (and similar) that detects whether
 * an external display is connected via the kernel DRM subsystem
 * (/sys/class/drm). When docked use is detected the fingerprint prompt
 * is skipped and the user falls through to password authentication.
 *
 * Framework 13 note:
 *   The laptop exposes displays via USB-C expansion cards as DisplayPort (DP):
 *     card1-DP-1 ... card1-DP-8   – external outputs (USB-C expansion cards)
 *     card1-eDP-1                  – built-in panel (always "connected", skip)
 *     card1-Writeback-1            – virtual connector, skip
 *   When a monitor is plugged in, one or more DP-N ports reports "connected".
 *   When disconnected all DP-N ports are "disconnected".
 *
 * Return values:
 *   PAM_SUCCESS  – external display connected → skip fingerprint, use password
 *   PAM_IGNORE – no external display        → allow fingerprint to proceed
 *
 * Build:
 *   gcc -shared -fPIC -o pam_dock_check.so pam_dock_check.c \
 *       -lpam -Wall -Wextra -O2
 */

#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <syslog.h>

#define DRM_CLASS_PATH "/sys/class/drm"

/*
 * Returns 1 if at least one *external* connector reports "connected".
 *
 * Skipped connectors:
 *   eDP-*       – embedded/built-in panel (always connected, not a monitor)
 *   Writeback-* – virtual writeback connector
 *
 * Matched connectors (any that survive the above filters):
 *   DP-*        – DisplayPort / USB-C (Framework 13 external outputs)
 *   HDMI-*      – HDMI (other laptops/desktops)
 */
static int external_display_connected(void)
{
    DIR *dir = opendir(DRM_CLASS_PATH);
    if (!dir) {
        syslog(LOG_AUTH | LOG_WARNING,
               "pam_dock_check: cannot open %s", DRM_CLASS_PATH);
        /* Fail safe: if sysfs unreadable, don't block fingerprint */
        return 0;
    }

    struct dirent *entry;
    int found = 0;

    while ((entry = readdir(dir)) != NULL && !found) {
        const char *name = entry->d_name;

        /* Sysfs class entries are symlinks; plain dirs are also fine */
        if (entry->d_type != DT_LNK && entry->d_type != DT_DIR)
            continue;

        /* Must contain a '-' – filters "card1", "renderD128", "version" */
        if (strchr(name, '-') == NULL)
            continue;

        /* Skip built-in panel */
        if (strstr(name, "eDP"))
            continue;

        /* Skip virtual writeback connector */
        if (strstr(name, "Writeback"))
            continue;

        /* Remaining entries are external outputs – check their status */
        char status_path[512];
        snprintf(status_path, sizeof(status_path),
                 "%s/%s/status", DRM_CLASS_PATH, name);

        FILE *f = fopen(status_path, "r");
        if (!f)
            continue;

        char buf[32] = {0};
        if (fgets(buf, sizeof(buf), f)) {
            buf[strcspn(buf, "\n")] = '\0';
            if (strcmp(buf, "connected") == 0) {
                syslog(LOG_AUTH | LOG_INFO,
                       "pam_dock_check: external display on %s", name);
                found = 1;
            }
        }
        fclose(f);
    }

    closedir(dir);
    return found;
}

/* ------------------------------------------------------------------ */
/*  PAM entry points                                                    */
/* ------------------------------------------------------------------ */

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags,
                                   int argc, const char **argv)
{
    (void)flags;
    (void)argc;
    (void)argv;

    if (external_display_connected()) {
        /*
         * Docked mode: tell PAM to skip fingerprint module entirely.
         * Execution falls through past the fingerprint module to password.
         */
        pam_syslog(pamh, LOG_INFO,
                   "pam_dock_check: external display present – using password");
        return PAM_SUCCESS;
    }

    /*
     * Laptop-only mode: no external display, allow fingerprint to proceed.
     */
    pam_syslog(pamh, LOG_INFO,
               "pam_dock_check: no external display – fingerprint permitted");
    return PAM_IGNORE;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags,
                               int argc, const char **argv)
{
    (void)pamh; (void)flags; (void)argc; (void)argv;
    return PAM_SUCCESS;
}

/* Required boilerplate stubs for a complete module */
PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
                                  int argc, const char **argv)
{
    (void)pamh; (void)flags; (void)argc; (void)argv;
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags,
                                    int argc, const char **argv)
{
    (void)pamh; (void)flags; (void)argc; (void)argv;
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags,
                                     int argc, const char **argv)
{
    (void)pamh; (void)flags; (void)argc; (void)argv;
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags,
                                  int argc, const char **argv)
{
    (void)pamh; (void)flags; (void)argc; (void)argv;
    return PAM_SUCCESS;
}
