echo "server program starts"

SERVER_PORT="6666"

# :<<COMMENT
numactl --cpunodebind=0 --membind=0  \
    ./single_queue_epoll_server
# COMMENT
