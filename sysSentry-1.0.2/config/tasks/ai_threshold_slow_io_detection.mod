[common]
enabled=yes
task_start=/usr/bin/python3 /usr/bin/ai_threshold_slow_io_detection
task_stop=pkill -f /usr/bin/ai_threshold_slow_io_detection
type=oneshot