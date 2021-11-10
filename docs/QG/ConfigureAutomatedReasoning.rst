.. _ConfigureAutomatedReasoning:

Configuring Automated Reasoning
################################################

.. contents::

Initial setup - Create a scenario
***********************************

1. Open the terminal and write:: 
    
    $ while true; do sleep 0; done &

2. To observe its effect on the system::  

    $ pmchart -t 0.5sec -c CPU &

3. Create a new chart showing the process context switch rate to the existing display::  

    kernel.all.pswitch

4. The above test case can be quite intrusive on low processor count machines, so remember to terminate it when you've finished this tutorial::  

    $ jobs
    [1]- Running     while true; do sleep 0; done &
    [2]+ Running     pmchart -t 0.5sec -c CPU &
    $ fg %1

However, you should leave it running throughout all of the tests below.

Using pmieconf and pmie
***********************************

1. Create your own pmie rules using pmieconf::  
    
    $ pmieconf -f myrules
    pmieconf> disable all
    pmieconf> enable cpu.context_switch
    pmieconf> modify global delta "5 sec"
    pmieconf> modify global holdoff ""
    pmieconf> modify global syslog_action no
    pmieconf> modify global user_action yes
    pmieconf> quit

This command sequence is:

- Inspecting the created file *myrules*
- Making reference to the *pmieconf* man page
- Exploring other *pmieconf* commands ("help" and "list" are useful in this context)

2. Run *pmie* rules using *pmieconf*, and see if the alarm messages appear on standard output::

   $ pmie -c myrules

3. Terminate *pmie* and use the reported values from *pmchart* to determine what the average rate of system calls is.  Then re-run *pmieconf* to adjust the threshold level up or down to alter the behaviour of *pmie*. Re-run *pmie*.

   .. sourcecode:: none

        $ pmieconf -f myrules
        pmieconf> modify cpu.context_switch threshold 5000    # <-- insert suitable value here
        pmieconf> quit
        $ pmie -c myrules

Monitoring state with the *shping* PMDA
*****************************************

1. Install *pmdashping* to record system state::

    # cd $PCP_PMDAS_DIR/shping
    # ./Install  


The default *shping* configuration is ``$PCP_PMDAS_DIR/shping/sample.conf``.
However, we can create a new configuration file, say ``$PCP_PMDAS_DIR/shping/my.conf``, with shell tag and command of the form:

.. sourcecode:: none

    no-pmie    test ! -f /tmp/no-pmie

2. Monitoring pmdashping to observe system state::

    $ pmval -t 5 shping.status

Open another command shell, first create the file */tmp/no-pmie*, wait ten seconds, and then remove the file. Observe what *pmval* reports in the other window. Terminate *pmval*.

Custom site rules with *pmieconf*
*********************************

1. Open an editor, edit the *pmieconf* output file created earlier, i.e. *myrules*. Append a new rule at the end (after the **END GENERATED SECTION** line), that is a copy of the **cpu.context_switch** rule.

2. To this new rule, add the following conjunct before the action line (containing ->), modify the message in the new rule's action to be different to the standard rule, make sure the threshold is low enough for the predicate to be true, and then save the file.

    .. sourcecode:: none

        && shping.status #'no-pmie' == 0

   
3. Re-run *pmieconf* to disable the standard rule::

    $ pmieconf -f myrules
    pmieconf> disable cpu.context_switch
    pmieconf> quit

4. Inspect the re-created file *myrules*. Check your new rule is still there and the standard rule has been removed.

5. Run *pmie* using *myrules*, and verify that your new alarm messages appear on standard output. In another window, create the file */tmp/no-pmie*, wait a while, then remove the file.

Notice there may be some delay between the creation or removal of */tmp/no-pmie* and the change in *pmie* behaviour.
