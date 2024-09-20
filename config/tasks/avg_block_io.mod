[common]
enabled=yes
task_start=/usr/bin/python3 /usr/bin/avg_block_io
task_stop=pkill -f /usr/bin/avg_block_io
type=oneshot
