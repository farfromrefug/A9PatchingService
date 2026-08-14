#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <android/log.h>
#include <ctype.h>
#include <sys/xattr.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/input.h>

static const char* kTAG = "a9EinkService";
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

#define SOCKET_NAME "0a9_eink_socket"
#define BUFFER_SIZE 512

/*
 * Double tap to wake.
 *
 * The touch controller keeps scanning while the display is off and reports ordinary BTN_TOUCH
 * events, but it never emits KEY_WAKEUP and the device is not marked as waking, so Android
 * discards those events before any of its own code sees them. Detecting the gesture here, from
 * the evdev node directly, sidesteps the input policy entirely. Reading the node does not
 * consume the events, so normal touch handling is unaffected.
 *
 * Armed and disarmed over the socket ("dt1"/"dt0") by the service, which knows the screen state.
 */
#define TOUCH_DEVICE_NAME "atmel"
#define TAP_MAX_DURATION_MS 250
#define TAP_MAX_MOVEMENT 40
#define DOUBLE_TAP_MAX_GAP_MS 400
#define KEYCODE_WAKEUP "224"

static volatile int gDoubleTapEnabled = 0;

static const char* theme_styles[] = {
        "TONAL_SPOT",
        "VIBRANT",
        "RAINBOW",
        "EXPRESSIVE",
        "FRUIT_SALAD",
        "SPRITZ"
};

int valid_number(const char *s) {
    if(strlen(s) > 4 || strlen(s) == 0)
        return 0;

    while (*s)
        if (isdigit(*s++) == 0) return 0;

    return 1;
}

int valid_hex_color(const char *s) {
    if (strlen(s) != 6)
        return 0;

    while (*s)
        if (!isxdigit(*s++)) return 0;

    return 1;
}

void sanitize_input(int theme_style_index, const char* hex_color, char* sanitized_theme_style, char* sanitized_hex_color) {
    if (theme_style_index < 0 || theme_style_index > 5) {
        theme_style_index = 5; // Default to monotone
    }
    strcpy(sanitized_theme_style, theme_styles[theme_style_index]);

    if (!valid_hex_color(hex_color)) {
        strcpy(sanitized_hex_color, "333333");
    } else {
        strcpy(sanitized_hex_color, hex_color);
    }
}

void generate_json_string(char* json_str, const char* theme_style, const char* hex_color) {
    sprintf(json_str, "{\"android.theme.customization.theme_style\":\"%s\",\"android.theme.customization.color_source\":\"preset\",\"android.theme.customization.system_palette\":\"%s\"}", theme_style, hex_color);
}

void execute_settings_command(const char* json_str) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/system/bin/settings", "settings", "put", "secure", "theme_customization_overlay_packages", json_str, (char *)NULL);
        LOGE("execlp failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        LOGE("fork failed: %s", strerror(errno));
    } else {
        wait(NULL);
    }
}

void applyThemeCustomization(int theme_style_index, const char* hex_color) {
    char sanitized_theme_style[20];
    char sanitized_hex_color[7];
    char json_str[BUFFER_SIZE];

    sanitize_input(theme_style_index, hex_color, sanitized_theme_style, sanitized_hex_color);
    generate_json_string(json_str, sanitized_theme_style, sanitized_hex_color);
    execute_settings_command(json_str);
}

void epdForceClear() {
    const char* filePath = "/sys/devices/platform/soc/soc:qcom,dsi-display-primary/epd_force_clear";
    int fd = open(filePath, O_WRONLY);
    if (fd == -1) {
        LOGE("Error writing to %s: %s\n", filePath, strerror(errno));
        return;
    }
    if (write(fd, "1", 1) == -1) {
        LOGE("Error writing to %s: %s\n", filePath, strerror(errno));
    }
    close(fd);
}

void epdCommitBitmap() {
    const char* filePath = "/sys/devices/platform/soc/soc:qcom,dsi-display-primary/epd_commit_bitmap";
    int fd = open(filePath, O_WRONLY);
    if (fd == -1) {
        LOGE("Error writing to %s: %s\n", filePath, strerror(errno));
        return;
    }
    if (write(fd, "1", 1) == -1) {
        LOGE("Error writing to %s: %s\n", filePath, strerror(errno));
    }
    close(fd);
}

void writeToEpdDisplayMode(const char* value) {
    const char* filePath = "/sys/devices/platform/soc/soc:qcom,dsi-display-primary/epd_display_mode";
    if(!valid_number(value)){
        LOGE("Error writing to %s: Invalid Number\n", filePath);
        return;
    }

    int fd = open(filePath, O_WRONLY);
    if (fd == -1) {
        LOGE("Error writing to %s: %s\n", filePath, strerror(errno));
        return;
    }
    if (write(fd, value, strlen(value)) == -1) {
        LOGE("Error writing to %s: %s\n", filePath, strerror(errno));
    }
    close(fd);
}

void setWhiteThreshold(const char* brightness) {
    const char* whiteThresholdPath ="/sys/devices/platform/soc/soc:qcom,dsi-display-primary/epd_white_threshold";
    if(!valid_number(brightness)){
        LOGE("Error writing to %s: Invalid Number\n", whiteThresholdPath);
        return;
    }
    int fd = open(whiteThresholdPath, O_WRONLY);
    if (fd == -1) {
        LOGE("Error writing to %s: %s\n", whiteThresholdPath, strerror(errno));
        return;
    }
    if (write(fd, brightness, strlen(brightness)) == -1) {
        LOGE("Error writing to %s: %s\n", whiteThresholdPath, strerror(errno));
    }
    close(fd);
}

void setBlackThreshold(const char* brightness) {
    const char* blackThresholdPath = "/sys/devices/platform/soc/soc:qcom,dsi-display-primary/epd_black_threshold";
    if(!valid_number(brightness)){
        LOGE("Error writing to %s: Invalid Number\n", blackThresholdPath);
        return;
    }
    int fd = open(blackThresholdPath, O_WRONLY);
    if (fd == -1) {
        LOGE("Error writing to %s: %s\n", blackThresholdPath, strerror(errno));
        return;
    }
    if (write(fd, brightness, strlen(brightness)) == -1) {
        LOGE("Error writing to %s: %s\n", blackThresholdPath, strerror(errno));
    }
    close(fd);
}

void setContrast(const char* brightness) {
    const char* contrastPath = "/sys/devices/platform/soc/soc:qcom,dsi-display-primary/epd_contrast";
    if(!valid_number(brightness)){
        LOGE("Error writing to %s: Invalid Number\n", contrastPath);
        return;
    }
    int fd = open(contrastPath, O_WRONLY);
    if (fd == -1) {
        LOGE("Error writing to %s: %s\n", contrastPath, strerror(errno));
        return;
    }
    if (write(fd, brightness, strlen(brightness)) == -1) {
        LOGE("Error writing to %s: %s\n", contrastPath, strerror(errno));
    }
    close(fd);
}

void writeLockscreenProp(const char* value) {
    if(!valid_number(value)){
        LOGE("Error setting static lockscreen: Invalid Number\n");
        return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        execl("/system/bin/setprop", "setprop", "sys.linevibrator_type", value, (char *)NULL);
        LOGE("execlp failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        LOGE("fork failed: %s", strerror(errno));
    } else {
        wait(NULL);
    }
}

void writeMaxBrightnessProp(const char* value) {
    if(!valid_number(value)){
        LOGE("Error setting static lockscreen: Invalid Number\n");
        return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        execl("/system/bin/setprop", "setprop", "sys.linevibrator_touch", value, (char *)NULL);
        LOGE("execlp failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        LOGE("fork failed: %s", strerror(errno));
    } else {
        wait(NULL);
    }
}

void writeWakeOnVolumeProp(const char* value) {
    if(!valid_number(value)){
        LOGE("Error setting static lockscreen: Invalid Number\n");
        return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        execl("/system/bin/setprop", "setprop", "sys.wakeup_on_volume", value, (char *)NULL);
        LOGE("execlp failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        LOGE("fork failed: %s", strerror(errno));
    } else {
        wait(NULL);
    }
}

static long long eventTimeMs(const struct timeval* tv) {
    return (long long)tv->tv_sec * 1000LL + (long long)tv->tv_usec / 1000LL;
}

static int openTouchDevice() {
    DIR* dir = opendir("/dev/input");
    if (dir == NULL) {
        LOGE("Error opening /dev/input: %s", strerror(errno));
        return -1;
    }

    int found = -1;
    struct dirent* entry;
    while (found < 0 && (entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
        int fd = open(path, O_RDONLY);
        if (fd == -1) {
            continue;
        }

        char name[128] = {0};
        if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0
                && strstr(name, TOUCH_DEVICE_NAME) != NULL) {
            LOGI("Double tap watching %s (%s)", path, name);
            found = fd;
        } else {
            close(fd);
        }
    }

    closedir(dir);
    if (found < 0) {
        LOGE("No input device matching '%s'", TOUCH_DEVICE_NAME);
    }
    return found;
}

static void injectWakeKey() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/system/bin/input", "input", "keyevent", KEYCODE_WAKEUP, (char *)NULL);
        LOGE("execl input failed: %s", strerror(errno));
        _exit(EXIT_FAILURE);
    } else if (pid < 0) {
        LOGE("fork failed: %s", strerror(errno));
    } else {
        waitpid(pid, NULL, 0);
    }
}

_Noreturn static void* doubleTapThread(void* arg) {
    (void)arg;

    int fd = -1;
    long long lastTapTime = -1;
    long long downTime = -1;
    int downX = 0, downY = 0, x = 0, y = 0, movement = 0, tracking = 0;

    while (1) {
        if (fd < 0) {
            fd = openTouchDevice();
            if (fd < 0) {
                sleep(5);
                continue;
            }
        }

        struct input_event ev;
        ssize_t num_read = read(fd, &ev, sizeof(ev));
        if (num_read != (ssize_t)sizeof(ev)) {
            LOGE("Touch device read failed: %s", strerror(errno));
            close(fd);
            fd = -1;
            tracking = 0;
            lastTapTime = -1;
            sleep(1);
            continue;
        }

        /* Keep draining the device while disarmed, so nothing is left buffered. */
        if (!gDoubleTapEnabled) {
            tracking = 0;
            lastTapTime = -1;
            continue;
        }

        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X) {
                x = ev.value;
            } else if (ev.code == ABS_MT_POSITION_Y) {
                y = ev.value;
            }
            if (tracking) {
                int dx = abs(x - downX);
                int dy = abs(y - downY);
                int distance = dx > dy ? dx : dy;
                if (distance > movement) {
                    movement = distance;
                }
            }
            continue;
        }

        if (ev.type != EV_KEY || ev.code != BTN_TOUCH) {
            continue;
        }

        long long now = eventTimeMs(&ev.time);
        if (ev.value == 1) {
            /* The position events of a packet arrive before BTN_TOUCH, so x/y are current. */
            downTime = now;
            downX = x;
            downY = y;
            movement = 0;
            tracking = 1;
            continue;
        }

        if (!tracking) {
            continue;
        }
        tracking = 0;

        int isTap = (now - downTime) <= TAP_MAX_DURATION_MS && movement <= TAP_MAX_MOVEMENT;
        if (!isTap) {
            lastTapTime = -1;
        } else if (lastTapTime > 0 && (now - lastTapTime) <= DOUBLE_TAP_MAX_GAP_MS) {
            LOGI("Double tap detected, waking");
            lastTapTime = -1;
            injectWakeKey();
        } else {
            lastTapTime = now;
        }
    }
}

void processCommand(const char* command) {
    if (strcmp(command, "cm") == 0) {
        epdCommitBitmap();
    } else if (strcmp(command, "r") == 0) {
        epdForceClear();
    } else if (strcmp(command, "c") == 0) {
        writeToEpdDisplayMode("515");
    } else if (strcmp(command, "b") == 0) {
        writeToEpdDisplayMode("513");
    } else if (strcmp(command, "s") == 0) {
        writeToEpdDisplayMode("518");
    } else if (strcmp(command, "p") == 0) {
        writeToEpdDisplayMode("521");
    } else if (strncmp(command, "stw", 3) == 0) {
        if(valid_number(command+3))
            setWhiteThreshold(command+3);
    } else if (strncmp(command, "stl", 3) == 0) {
        if(valid_number(command+3))
            writeLockscreenProp(command+3);
    } else if (strncmp(command, "stb", 3) == 0) {
        if(valid_number(command+3))
            setBlackThreshold(command+3);
    } else if (strncmp(command, "sco", 3) == 0) {
        if(valid_number(command+3))
            setContrast(command+3);
    } else if (strncmp(command, "smb", 3) == 0) {
        if(valid_number(command+3))
            writeMaxBrightnessProp(command+3);
    } else if (strncmp(command, "wov", 3) == 0) {
        if(valid_number(command+3))
            writeWakeOnVolumeProp(command+3);
    } else if (strncmp(command, "dt", 2) == 0) {
        if(valid_number(command+2))
            gDoubleTapEnabled = atoi(command+2) != 0;
    } else if (strncmp(command, "theme", 5) == 0) {
        int theme_style_index;
        char hex_color[7];
        if (sscanf(command + 5, "%d %6s", &theme_style_index, hex_color) == 2) {
            applyThemeCustomization(theme_style_index, hex_color);
        } else {
            LOGE("Invalid theme command format");
        }
    } else {
        LOGE("Unknown command: %s", command);
    }
}

_Noreturn void setupServer() {
    int server_sockfd, client_sockfd;
    struct sockaddr_un server_addr;
    char buffer[BUFFER_SIZE];
    socklen_t socket_length = sizeof(server_addr.sun_family) + strlen(SOCKET_NAME);

    server_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        LOGE("Socket creation failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_NAME, sizeof(server_addr.sun_path) - 1);
    server_addr.sun_path[0] = 0;

    if (bind(server_sockfd, (struct sockaddr*)&server_addr, socket_length) < 0) {
        LOGE("Socket bind failed: %s", strerror(errno));
        close(server_sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sockfd, 50) < 0) {
        LOGE("Socket listen failed: %s", strerror(errno));
        close(server_sockfd);
        exit(EXIT_FAILURE);
    }

    LOGI("Server started listening.");

    while (1) {
        client_sockfd = accept(server_sockfd, NULL, NULL);
        if (client_sockfd < 0) {
            LOGE("Socket accept failed: %s", strerror(errno));
            continue;
        }

        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            ssize_t num_read = read(client_sockfd, buffer, BUFFER_SIZE - 1);
            if (num_read > 0) {
                buffer[num_read] = '\0';
                char* cmd = strtok(buffer, "\n");
                while (cmd != NULL) {
                    processCommand(cmd);
                    cmd = strtok(NULL, "\n");
                }
            } else if (num_read == 0) {
                LOGI("Client disconnected");
                break;
            } else {
                LOGE("Socket read failed: %s", strerror(errno));
                break;
            }
        }

        close(client_sockfd);
    }

    close(server_sockfd);
}

int main(void) {
    signal(SIGHUP, SIG_IGN);
    /* SIGPIPE would otherwise kill the daemon if a client goes away mid-write. */
    signal(SIGPIPE, SIG_IGN);

    pthread_t doubleTapTid;
    if (pthread_create(&doubleTapTid, NULL, doubleTapThread, NULL) != 0) {
        LOGE("Failed to start double tap thread: %s", strerror(errno));
    } else {
        pthread_detach(doubleTapTid);
    }

    setupServer();
    return 0;
}