## About

These are simple Linux kernel modules written for educational purposes, some of
which were written while answering/browsing StackOverflow questions on the
`[linux-kernel]` tag.

See "Building" section below for build instructions.

| Module                                             | Description                                                                | Kernel (tested on)    | SO question?                                 |
|:---------------------------------------------------|:---------------------------------------------------------------------------|:----------------------|:---------------------------------------------|
| [`arm64/syscall_hijack.c`](arm64/syscall_hijack.c) | Test syscall table hijacking on arm64                                      | 4.19                  | [61247838][q61247838]                        |
| [`arm64/undef_instr.c`](arm64/undef_instr.c)       | Test kernel undefined instruction handler on arm64                         | 4.19                  | [61238959][q61238959]                        |
| [`cpufreq.c`](cpufreq.c)                           | Get CPU frequency for currently online CPUs                                | 5.10                  | [64111116][q64111116]                        |
| [`cpuinfo.c`](cpuinfo.c)                           | Get CPU core ID from current CPU ID                                        | 5.10                  | [61349444][q61349444]                        |
| [`datetime.c`](datetime.c)                         | Get current date and time from kernel space taking into account time zone  | 5.10                  | -                                            |
| [`enum_pids.c`](enum_pids.c)                       | Enumerate all the tasks that have a given PID as pid, tgid, pgid or sid    | 5.10                  | [67235938][q67235938], [71204947][q71204947] |
| [`find_root_dev.c`](find_root_dev.c)               | Find the device where root (/) is mounted and its name                     | 4.19                  | [60878209][q60878209]                        |
| [`kallsyms.c`](kallsyms.c)                         | Lookup and grep kallsyms from kernel space                                 | 4.19, 5.4, 5.10, 5.18 | [70930059][q70930059]                        |
| [`page_table_walk.c`](page_table_walk.c)           | Walk user/kernel page tables and dump entries given a virtual address      | 5.10, 5.17, 6.12      | -                                            |
| [`reboot_notifier.c`](reboot_notifier.c)           | Test waiting for a critical job (kthread) to finish before poweroff/reboot | 5.10                  | [64670766][q64670766]                        |
| [`task_bfs_dfs.c`](task_bfs_dfs.c)                 | Iterate over a task's children tree using BFS or DFS                       | 5.10, 5.17            | [19208487][q19208487], [61201560][q61201560] |
| [`task_rss.c`](task_rss.c)                         | Calculare task RSS of all running tasks                                    | 5.6, 5.10, 5.17       | [67224020][q67224020]                        |
| [`task_rss_from_pid.c`](task_rss_from_pid.c)       | Calculare task RSS given an userspace PID                                  | 5.6, 5.10, 5.17       | [67224020][q67224020]                        |
| [`test_chardev.c`](test_chardev.c)                 | Test character device kernel APIs                                          | 5.8, 5.10             | -                                            |
| [`test_hashtable.c`](test_hashtable.c)             | Test kernel hashtable API                                                  | 5.10                  | [60870788][q60870788]                        |

## Building

Modules should compile for Linux x86_64 kernel versions listed in the above
table. Since kernel compatibility varies, **it is impossible to build
*all* modules at once** with a simple `make -j`. Specific modules can be built
specifying their name in `ONLY=`:

```bash
make KDIR=path/to/kernel/dir ONLY='cpuinfo datetime'
```

Modules in the `arm64` directory are ARM64-specific, so either use an ARM64
machine or cross-compile specifying your toolchain prefix:

```bash
make KDIR=path/to/kernel/dir CROSS_COMPILE=aarch64-linux-gnu-
```

[q19208487]: https://stackoverflow.com/q/19208487/3889449
[q60870788]: https://stackoverflow.com/q/60870788/3889449
[q60878209]: https://stackoverflow.com/q/60878209/3889449
[q61201560]: https://stackoverflow.com/q/61201560/3889449
[q61238959]: https://stackoverflow.com/q/61238959/3889449
[q61247838]: https://stackoverflow.com/q/61247838/3889449
[q61349444]: https://stackoverflow.com/q/61349444/3889449
[q64111116]: https://stackoverflow.com/q/64111116/3889449
[q64670766]: https://stackoverflow.com/q/64670766/3889449
[q67224020]: https://stackoverflow.com/q/67224020/3889449
[q67235938]: https://stackoverflow.com/q/67235938/3889449
[q70930059]: https://stackoverflow.com/q/70930059/3889449
[q71204947]: https://stackoverflow.com/q/71204947/3889449
