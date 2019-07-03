echo "client program starts"

NUM_TASK_PER_THREAD="10"

# :<<COMMENT
numactl --physcpubind=1 \
    ./client_pool
# COMMENT
