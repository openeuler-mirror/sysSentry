[common]
enabled=yes
task_start=/usr/bin/python3 /usr/bin/ai_block_io
task_stop=pkill -f /usr/bin/ai_block_io
type=oneshot