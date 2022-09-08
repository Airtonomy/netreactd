// From https://olegkutkov.me/2018/02/14/monitoring-linux-networking-state-using-netlink/

// C headrs
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Linux headers
#include <arpa/inet.h>
#include <errno.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    size_t timeoutSeconds;
    const char *script;
    bool volatile *running;
} config_t;

static bool strIsEmpty(char const *const str) {
    return str == NULL || strlen(str) == 0;
}

static void *scriptThread(/*char const *const*/ void *script_) {
    char const *const script = (char const *const)script_;
    if (!strIsEmpty(script_)) {
        printf("Running script: %s\n", script);
        int const returnCode = system(script);
        printf("Script finished with code %d\n", returnCode);
    }
    return NULL;
}

static void *timeoutThread(/*config_t const *const*/ void *config_) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    config_t const *const config = (config_t const *const)config_;

    printf("Waiting %lds\n", config->timeoutSeconds);
    usleep(config->timeoutSeconds * 1000000);

    pthread_t threadId;
    pthread_create(&threadId, NULL, scriptThread, (void*)config->script);

    *config->running = false;
    return NULL;
}

// little helper to parsing message using netlink macroses
static void parseRtattr(struct rtattr *tb[], int const max, struct rtattr *rta, int len)
{
    memset(tb, 0, sizeof(struct rtattr *) * (max + 1));

    while (RTA_OK(rta, len)) {  // while not end of the message
        if (rta->rta_type <= max) {
            tb[rta->rta_type] = rta; // read attr
        }
        rta = RTA_NEXT(rta,len);    // get next attr
    }
}

static int main_loop(char const *const ifTarget, size_t const timeoutSeconds, char const *const newLinkScript, char const *const upScript, char const *const downScript) {
    bool volatile threadActive = false;
    config_t const threadConfig = {
        .timeoutSeconds = timeoutSeconds,
        .script = upScript,
        .running = &threadActive,
    };
    pthread_t threadId;
    memset(&threadId, 0, sizeof(threadId));

    int const fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);   // create netlink socket

    if (fd < 0) {
        printf("Failed to create netlink socket: %s\n", (char*)strerror(errno));
        return 1;
    }

    struct sockaddr_nl local;   // local addr struct
    char buf[8192];             // message buffer
    struct iovec iov;           // message structure
    iov.iov_base = buf;         // set message buffer as io
    iov.iov_len = sizeof(buf);  // set size

    memset(&local, 0, sizeof(local));

    local.nl_family = AF_NETLINK;       // set protocol family
    local.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE;   // set groups we interested in
    local.nl_pid = getpid();    // set out id using current process id

    // initialize protocol message header
    struct msghdr msg = {
        .msg_name = &local,                  // local address
        .msg_namelen = sizeof(local),        // address size
        .msg_iov = &iov,                     // io vector
        .msg_iovlen = 1,                     // io size
    };

    if (bind(fd, (struct sockaddr*)&local, sizeof(local)) < 0) {     // bind socket
        printf("Failed to bind netlink socket: %s\n", (char*)strerror(errno));
        close(fd);
        return 1;
    }

    // read and parse all messages from the
    while (true) {
        ssize_t status = recvmsg(fd, &msg, MSG_DONTWAIT);

        //  check status
        if (status < 0) {
            if (errno == EINTR || errno == EAGAIN)
            {
                usleep(250000);
                continue;
            }

            printf("Failed to read netlink: %s", (char*)strerror(errno));
            continue;
        }

        if (msg.msg_namelen != sizeof(local)) { // check message length, just in case
            printf("Invalid length of the sender address struct\n");
            continue;
        }

        // message parser
        struct nlmsghdr const *h;

        for (h = (struct nlmsghdr const*)buf; status >= (ssize_t)sizeof(*h); ) {   // read all messagess headers
            int const len = h->nlmsg_len;
            int const l = len - sizeof(*h);
            char const *ifName = NULL;

            if ((l < 0) || (len > status)) {
                printf("Invalid message length: %i\n", len);
                continue;
            }

            // now we can check message type
            if ((h->nlmsg_type == RTM_NEWROUTE) || (h->nlmsg_type == RTM_DELROUTE)) { // some changes in routing table
                printf("Routing table was changed\n");
            } else {    // in other case we need to go deeper
                 // structure for network interface info
                struct ifinfomsg const *const ifi = (struct ifinfomsg const *const)NLMSG_DATA(h);    // get information about changed network interface

                struct rtattr *tb[IFLA_MAX + 1];
                parseRtattr(tb, IFLA_MAX, IFLA_RTA(ifi), h->nlmsg_len);  // get attributes

                if (tb[IFLA_IFNAME]) {  // validation
                    ifName = (char const*)RTA_DATA(tb[IFLA_IFNAME]); // get network interface name
                }

                bool isUp = ifi->ifi_flags & IFF_UP; // get UP flag of the network interface
                char const *const ifUpp = isUp
                    ? "UP"
                    : "DOWN";

                bool isRunning = ifi->ifi_flags & IFF_RUNNING; // get RUNNING flag of the network interface
                char const *const ifRunn = isRunning
                    ? "RUNNING"
                    : "NOT RUNNING";

                char ifAddress[256];    // network addr
                struct rtattr *tba[IFA_MAX+1];

                // structure for network interface data
                struct ifaddrmsg const *const ifa = (struct ifaddrmsg const *const)NLMSG_DATA(h); // get data from the network interface

                parseRtattr(tba, IFA_MAX, IFA_RTA(ifa), h->nlmsg_len);

                if (tba[IFA_LOCAL]) {
                    inet_ntop(AF_INET, RTA_DATA(tba[IFA_LOCAL]), ifAddress, sizeof(ifAddress)); // get IP addr
                }

                bool isDown = false;
                switch (h->nlmsg_type) { // what actually happened?
                    case RTM_DELADDR:
                        printf("Interface %s: address was removed\n", ifName);
                        break;

                    case RTM_DELLINK:
                        printf("Network interface %s was removed\n", ifName);
                        isDown = true;
                        break;

                    case RTM_NEWLINK:
                        printf("New network interface %s, state: %s %s\n", ifName, ifUpp, ifRunn);
                        
                        isDown = !isUp || !isRunning;
                        if (!threadActive && isUp && isRunning && strcmp(ifName, ifTarget) == 0) {
                            // if there is a newLinkScript run and break on success
                            if (!strIsEmpty(newLinkScript)) {
                                printf("Running NEWLINK_SCRIPT script\n");
                                int const returnCode = system(newLinkScript);
                                printf("Script finished with code %d\n", returnCode);
                                if (returnCode == 0) {
                                    break;
                                }
                            }
                            
                            // did not break from script
                            pthread_create(&threadId, NULL, timeoutThread, (void*)&threadConfig);
                            threadActive = true;
                        
                        } else if (threadActive && isDown && strcmp(ifName, ifTarget) == 0) {
                            printf("Target interface went down while thread was running\n");
                            pthread_cancel(threadId);
                            threadActive = false;
                        }
                        break;

                    case RTM_NEWADDR:
                        printf("Interface %s: new address was assigned: %s\n", ifName, ifAddress);

                        // If the string starts with 169. it is a link local and we don't want a
                        // link local address to be treated as a real IP address
                        if (threadActive && strcmp(ifName, ifTarget) == 0 && strncmp(ifAddress, "169.", 4) != 0) {
                            printf("Canceling timeout\n");
                            pthread_cancel(threadId);
                            threadActive = false;
                        }
                        break;
                }

                if (isDown && strcmp(ifName, ifTarget) == 0 && !strIsEmpty(downScript)) {
                    printf("Running down script\n");
                    pthread_create(&threadId, NULL, scriptThread, (void*)downScript);
                }
            }

            status -= NLMSG_ALIGN(len); // align offsets by the message length, this is important

            h = (struct nlmsghdr const*)((char*)h + NLMSG_ALIGN(len));    // get next message
        }

        usleep(250000); // sleep for a while
    }

    close(fd);  // close socket

    return 0;
}

static void sigintHandler(int const sig)
{
    exit(0);
}

int main() {
    signal(SIGINT, sigintHandler);

    char const* const timeoutStr = getenv("NETREACT_TIMEOUT");
    int const timeout = timeoutStr == NULL || strlen(timeoutStr) == 0
        ? 1
        : atoi(timeoutStr);
    char const* const newLinkScript = getenv("NEWLINK_SCRIPT");
    char const* const upScript = getenv("NETREACT_UP_SCRIPT");
    char const* const downScript = getenv("NETREACT_DOWN_SCRIPT");
    char const* const targetIf = getenv("NETREACT_IF");
    if (strIsEmpty(upScript) || strIsEmpty(targetIf)) {
        printf("USAGE: NETREACT_IF=eth0 NETREACT_TIMEOUT=3 [NEWLINK_SCRIPT=./do_something.sh]  NETREACT_UP_SCRIPT=./do_something.sh [NETREACT_DOWN_SCRIPT=./do_something.sh] command\n");
        return 1;
    }

    printf("Watching interface %s with a timeout of %ds\n", targetIf, timeout);
    return main_loop(targetIf, timeout, newLinkScript, upScript, downScript);
}
