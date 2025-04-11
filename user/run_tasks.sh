#!/bin/bash

# Script to launch a list of tasks
set -e

if [ $# -lt 2 ]; then
    echo "Usage: $0 <job file> <policy> [cpu_list]"
    echo "Policies: 1=FIFO, 7=FREEZER, 0=DEFAULT"
    echo "cpu_list: comma-separated list of CPUs to use (e.g. 0,1,2,3)"
    exit 1
fi

FILE=$1
POLICY=$2
CPU_LIST=$3

if [ -z "$CPU_LIST" ]; then
    USE_TASKSET=0
else
    USE_TASKSET=1
fi

# Change ourselves to SCHED_FIFO at high priority
# to ensure we get CPU time to launch all tasks
sudo chrt --fifo -p 99 $$

# Launch the eBPF trace.bt in the background (if not already running)
sudo bpftrace trace.bt > part5_1cpu_logs/${FILE%.txt}_policy${POLICY}.log &
TRACE_PID=$!

# Give trace.bt time to start
sleep 1

# Launch each Fibonacci task
pids=""
while read line; do
    if [ $USE_TASKSET -eq 1 ]; then
        # Assign to specific CPUs
        if [ $POLICY -eq 1 ]; then
            # For FIFO policy, use chrt
            sudo taskset -c $CPU_LIST sudo chrt --fifo 1 ./fibonacci $line 0 &
        else
            # For other policies, set via fibonacci program
            sudo taskset -c $CPU_LIST ./fibonacci $line $POLICY &
        fi
    else
        # No CPU constraints
        if [ $POLICY -eq 1 ]; then
            # For FIFO policy, use chrt
            sudo chrt --fifo 1 ./fibonacci $line 0 &
        else
            # For other policies, set via fibonacci program
            ./fibonacci $line $POLICY &
        fi
    fi
    pids="$pids $!"
done < $FILE

# Wait for all tasks to complete
wait $pids

# Give trace.bt time to finish writing logs
sleep 2

# Kill the trace.bt process
kill $TRACE_PID 2>/dev/null || true

# Calculate metrics from the log
LOG_FILE=${FILE%.txt}_policy${POLICY}.log
echo "Results for policy $POLICY with task set $FILE:"

# Calculate average completion time
TOTAL=0
COUNT=0
while IFS="," read -r comm pid runq_ms total_ms; do
    if [ "$comm" != "COMM" ]; then
        TOTAL=$((TOTAL + total_ms))
        COUNT=$((COUNT + 1))
    fi
done < $LOG_FILE

if [ $COUNT -gt 0 ]; then
    AVG=$((TOTAL / COUNT))
    echo "Average completion time: $AVG ms"
else
    echo "No tasks were recorded"
fi

# Calculate tail completion time (99th percentile)
# First, extract all completion times and sort them numerically
echo "Extracting completion times..."
grep "fibonacci" $LOG_FILE | cut -d',' -f4 | sort -n > /tmp/completion_times.txt

# Calculate the 99th percentile
TOTAL_LINES=$(wc -l < /tmp/completion_times.txt)
PERCENTILE_INDEX=$(( TOTAL_LINES * 99 / 100 ))
if [ $PERCENTILE_INDEX -ge $TOTAL_LINES ]; then
    PERCENTILE_INDEX=$((TOTAL_LINES - 1))
fi
if [ $PERCENTILE_INDEX -lt 1 ]; then
    PERCENTILE_INDEX=0
fi

TAIL_TIME=$(sed -n "$((PERCENTILE_INDEX + 1))p" /tmp/completion_times.txt)
echo "Tail completion time (99th percentile): $TAIL_TIME ms"
echo "Total tasks: $COUNT"

# Clean up
rm -f /tmp/completion_times.txt