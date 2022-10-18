## About

These are simple userspace programs written for educational purposes to test
different Linux functionalities and features, some of which were written while
answering StackOverflow questions.

| Program                                              | Description                                                                                       | SO question?                                             |
|:-----------------------------------------------------|:--------------------------------------------------------------------------------------------------|:---------------------------------------------------------|
| [`alias_existing_page.c`](alias_existing_page.c)     | Map an existing page to a different vaddr using `/dev/mem` and `/proc/PID/pagemap`                | [67781437](https://stackoverflow.com/q/67781437/3889449) |
| [`ftrace_helper.c`](ftrace_helper.c)                 | Automate kernel function tracer usage through tracefs with minimal trace output noise             | -                                                        |
| [`inherit_capability.c`](inherit_capability.c)       | Test preserving a privileged capability after dropping privileges                                 | -                                                        |
| [`pageinfo.c`](pageinfo.c)                           | Dump information from `/proc/[pid]/pagemap` and `/proc/kpage{flags,count}` in human readable form | -                                                        |
| [`test_scaling_governor.c`](test_scaling_governor.c) | Test scaling governor behavior when sleeping before a CPU-intensive task                          | [60351509](https://stackoverflow.com/q/60351509/3889449) |
| [`x86_rtm_page_fault.c`](x86_rtm_page_fault.c)       | Use x86 transactional memory to detect page faults and dirty pages                                | -                                                        |

## Building

A simple `make` should suffice. To build a specific program simply specify it as
a target to make without the `.c` extension:

```bash
make alias_existing_page
```

Note: `inherit_capability.c` needs `libcap-dev`.
