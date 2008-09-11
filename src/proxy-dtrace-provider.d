
provider mysqlproxy {
    /**
     * fires when the internal chassis state machine arrives at a new state.
     * @param event_fd File descriptor this event fired on
     * @param events Flags which events happened (from libevent)
     * @param state Connection state, enum state from network-mysqld.h
     */
    probe state__change(int, short, int);
};
