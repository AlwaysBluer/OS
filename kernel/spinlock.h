// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
#ifdef LAB_LOCK
  int nts;
  int n;
#endif
};

//对锁的一点理解：锁的应用场景是多核多线程环境，用来保证只有一个线程进入临界区，就是防止多个线程获取资源时的冲突，
//acquire(lock)的过程,其实就是，locked置为1表示上锁，cpu置为当前cpu，从而阻止其它cpu上的线程访问上锁的资源
