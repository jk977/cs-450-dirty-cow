# Dirty COW Presentation

## Background

* Copy-On-Write (COW)
    * When a program requests a copy of a resource, the kernel doesn't actually copy the resource, the original and the copy both refer to the same resource
    * The kernel will wait to make a copy until a program writes to the resource
* When writing to a file, kernel [doesn't immediately write][1] changes to disk
    * Page cache is kept in RAM to speed things up
    * Pages with changes are marked dirty, scheduled to write to disk
* Disk is updated via [dedicated kernel threads][2]

[1]: http://sylab-srv.cs.fiu.edu/lib/exe/fetch.php?media=paperclub:lkd3ch16.pdf
[2]: https://www.cs.cmu.edu/~mukesh/hacks/spindown/t1.html

## Vulnerability

Exploit is caused by a [race condition in handling copy-on-write][3].

### Exploit Steps
1. Open a read-only file
2. Create a private memory mapping of the file
    * Set up using Copy-On-Write
3. Spawn a thread that repeatedly writes to the private mapping
    * Writing to the memory mapping consists of two non-atomic steps
        1. Locate the physical address
        2. Write to the physical address
4. Spawn another thread that repeatedly tells the kernel we don't expect to access the mapping
    * If we manage to tell the kernel to discard the memory mapping between the two write steps, the kernel will write to the read-only file instead

### Implications
You can write to any file you have read access to.

[3]: https://www.cs.toronto.edu/~arnold/427/18s/427_18S/indepth/dirty-cow/index.html

## Mitigation

* First attempted to be fixed in a 2005 patch ([commit 4ceb5db9757a][4]) by Torvalds, but caused other issues
* Fixed by Torvalds in 2016 ([commit 19be0eaffa3a][5]) by introducing a paging flag specifically for COWs
* On [vulnerable systems][6]:
    * Update or patch kernel
    * Use tools to monitor filesystem for unexpected changes

[4]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=4ceb5db9757aaeadcf8fbbf97d76bd42aa4df0d6
[5]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=19be0eaffa3ac7d8eb6784ad9bdbc7d67ed8e619
[6]: https://www.digitalocean.com/community/tutorials/how-to-protect-your-server-against-the-dirty-cow-linux-vulnerability

## Sidenotes

### Kernel crash

* Using the implemented exploit will cause the kernel to crash shortly after
* Crash can be prevented by stopping dirty writebacks, which prevents the changes from being written to disk
    * Done by setting [`/proc/sys/vm/dirty_writeback_centisecs`][7] to 0

[7]: https://www.kernel.org/doc/Documentation/sysctl/vm.txt
