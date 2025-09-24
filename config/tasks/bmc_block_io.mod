[common]
enabled=yes
task_start=/usr/bin/bmc_block_io
task_stop=kill $pid
type=oneshot
alarm_id=1002
alarm_clear_time=5