unix环境高级编程第八章
{
    8.0 本章学习内容
    {
        1.如何创建进程、执行程序和终止进程
        2.进程属性的各种ID
        3.进程属性如何受到进程控制原语的影响
        4.介绍解释器文件
        5.system函数
        6.进程会计机制
    }

    8.1 如何创建进程、执行进程和终止进程
    {
        fork函数
        {
            fork函数被调用一次，返回两次，子进程的返回值是0，而父进程的返回值是新子进程的进程ID。
            子进程可以通过调用getppid来获得其父进程的进程ID。

            子进程将获得父进程数据空间、堆和栈的副本，但父、子进程不共享这些存储空间部分。
            父、子进程共享正文段。

            test_fork.c中演示了fork函数，展示了子进程对变量所做的改变并不影响父进程中该变量的值。
            一般来说，fork之后父进程先执行还是子进程先执行是不确定的。
            如果要求父、子进程之间相互同步，则要求某种形式的进程间通信。
        }

        vfork函数
        {
            vfork函数的调用序列和返回值与fork相同
            vfork创建的新进程的目的是exec一个新程序。
            vfork函数保证子进程先运行，只有当子进程调用了exec或者exit函数之后父进程才能被调度运行。
            vfork函数在调用exec或exit之前，它将在父进程的地址空间中运行，所以可能改变父进程的变量。
            在test_fork.c中演示了vfork函数。
        }

        5种正常终止方式
        {
            1.执行return语句，等效于调用exit
            2.调用exit函数。这个会涉及到atexit函数，最后会调用各终止处理程序，然后关闭所有标准I/O流。
            3.调用_exit或_Exit函数。
            4.进程的最后一个线程执行返回语句，此时进程以终止状态0返回。
            5.进程的最后一个线程执行pthread_exit函数。
        }

        3种异常终止方式
        {
            1.调用abort，然后产生SIGABRT信号。
            2.当进程接收到某些信号时，将终止进程。
            3.最后一个线程对"取消"请求做出响应。
        }

        如果父进程在子进程之前终止，则子进程的父进程都改变为init进程。该过程称为进程领养。
        在一个进程终止时，内核逐个检查所有活动进程，判断该进程是否是正要终止的进程的子进程。如果是，就将该子进程的父进程ID更改为1。

        如果子进程在父进程之前终止，父进程是如何检查子进程的终止状态的
        {
            内核为每个终止子进程保存了一定量的信息。
            当终止进程的父进程调用wait或waitpid时，可以得到这些信息。
            这些信息包括进程ID、该进程的终止状态以及该进程使用的CPU时间总量。
        }

        僵死进程：一个已经终止的进程，但是它的父进程尚未对其进行善后处理。
        ps(1)命令将僵死进程的状态打印为Z。
        如果编写一个长期运行的程序，它调用fork产生了许多子进程，那么如果父进程等待取得子进程的终止状态，否则这些子进程终止后将会变成僵死进程。
        由init进程领养的进程在终止时不会变成僵死进程。因为init被编写成无论何时只要一个子进程终止，init就会调用一个wait函数去取得其终止状态。

        调用wait或waitpid的进程可能遇到的情况
        {
            1.如果其所有子进程都还在运行，则阻塞。
            2.如果一个子进程已经终止，正等待父进程获取其终止状态，则取得该子进程终止状态立即返回。
            3.如果它没有任何子进程，则立即出错返回。

            pid_t wait(int *statloc);
            pid_t wait(pid_t pid, int *statloc, int options);
            statloc是一个整型指针。如果statloc不是空指针，则终止进程的终止状态就会存放在它所指向的单元内。
            waitpid有一个选项能让调用者不阻塞，而wait没有。而且waitpid可以控制它所等待的进程。

            在test_wait.c中演示了wait和waitpid所返回的终止状态以及相应用于检查的宏。
        }

        对于僵死进程，如果一个进程fork一个子进程，但不要它等待子进程终止，也不希望子进程处于僵死状态直到父进程终止，实现这一要求的技巧就是调用fork两次。
        在twice_fork.c中演示了该技巧。

        另一个取进程终止状态的函数waitid
        {
            int waitid(idtype_t idtype, id_t id, siginfo *infop, int options);

            idtype_t常量
            {
                1.P_PID，等待一个特定的进程，id参数包含要等待的子进程的进程ID
                2.P_PGID，等待一个特定进程组中的任一子进程，id包含要等待的子进程的进程组ID
                3.P_ALL，等待任一子进程，忽略id参数
            }

            infop参数是指向siginfo结构的指针，该结构包含了有关引起相关子进程状态改变的生成信号的详细信息。
        }
    }

    8.2 进程属性的各种ID
    {
        ID为0的进程通常是调度进程，通常被称为交换进程。
        交换进程是内核的一部分，并不执行任何磁盘上的程序，因此也被称为系统进程。
        ID为1的进程通常是init进程，在自举过程结束时由内核调用。
        ID为2的进程是页守护进程，负责支持虚拟存储系统的分页操作。
    }

    8.3 进程属性如何受到进程控制原语的影响
    {
        竞争条件
        {
            定义：当多个进程都企图对共享数据进行某种处理，但是最后的结果又取决于进程运行的顺序，那么这就产生了竞争条件。

            举个例子：如果在fork之后，程序运行的情况依赖于父进程先运行还是子进程先运行，那么这里就产生了竞争条件。
            再举个例子
            {
                父进程要用子进程ID更新日志文件中的一个记录，而子进程要为父进程创建一个文件。
                这个例子要求每个进程在执行完它的一套初始化操作后通知对方，并且在继续运行之前，要等待另一方完成其初始化操作。
                在test_condition_race.c中演示了这个例子。
            }
        }
    }
}