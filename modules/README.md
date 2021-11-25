## About

These are simple Linux kernel modules written for educational purposes, some of
which were written while answering StackOverflow questions on the
`[linux-kernel]` tag.

| Module                                             | Description                                                                | SO question?                                             |
|:---------------------------------------------------|:---------------------------------------------------------------------------|:---------------------------------------------------------|
| [`arm64/syscall_hijack.c`](arm64/syscall_hijack.c) | Test syscall table hijacking on arm64                                      | [61247838](https://stackoverflow.com/q/61247838/3889449) |
| [`arm64/undef_instr.c`](arm64/undef_instr.c)       | Test kernel undefined instruction handler in arm64                         | [61238959](https://stackoverflow.com/q/61238959/3889449) |
| [`cpufreq.c`](cpufreq.c)                           | Get CPU frequency for currently online CPUs                                | [64111116](https://stackoverflow.com/q/64111116/3889449) |
| [`cpuinfo.c`](cpuinfo.c)                           | Get CPU core ID from current CPU ID                                        | [61349444](https://stackoverflow.com/q/61349444/3889449) |
| [`datetime.c`](datetime.c)                         | Get current date and time from kernel space taking into account time zone  | -                                                        |
| [`enum_pids.c`](enum_pids.c)                       | Enumerate all the tasks that have a given PID as pid, tgid, pgid or sid    | [67235938](https://stackoverflow.com/q/67235938/3889449) |
| [`find_root_dev.c`](find_root_dev.c)               | Find the device where root (/) is mounted and its name                     | [60878209](https://stackoverflow.com/q/60878209/3889449) |
| [`kallsyms.c`](kallsyms.c)                         | Lookup kallsyms from kernel space                                          | -                                                        |
| [`reboot_notifier.c`](reboot_notifier.c)           | Test waiting for a critical job (kthread) to finish before poweroff/reboot | [64670766](https://stackoverflow.com/q/64670766/3889449) |
| [`task_bfs.c`](task_bfs.c)                         | Iterate over a task's children tree using BFS                              | [61201560](https://stackoverflow.com/q/61201560/3889449) |
| [`task_rss.c`](task_rss.c)                         | Calculare task RSS of all running tasks                                    | [67224020](https://stackoverflow.com/q/67224020/3889449) |
| [`task_rss_from_pid.c`](task_rss_from_pid.c)       | Calculare task RSS given an userspace PID                                  | [67224020](https://stackoverflow.com/q/67224020/3889449) |
| [`test_chardev.c`](test_chardev.c)                 | Test character device kernel APIs                                          | -                                                        |
| [`test_hashtable.c`](test_hashtable.c)             | Test kernel hashtable API                                                  | [60870788](https://stackoverflow.com/q/60870788/3889449) |

## Building

All modules should compile for Linux v4.19 x86_64 with a simple `make`:

```bash
make KDIR=path/to/kernel/dir -j
```

Specific modules can be built specifying their name in `ONLY=`:

```bash
make KDIR=path/to/kernel/dir ONLY='cpuinfo datetime'
```

Modules in the `arm64` folder are ARM64-specific, so either use an ARM64 machine
or cross-compile with `CROSS_COMPILE=aarch64-linux-gnu-` (default behavior):

```bash
cd arm64
make KDIR=path/to/kernel/dir -j
```
