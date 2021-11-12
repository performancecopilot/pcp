.. _AutomateProblemDetection:

Automate performance problem detection
################################################

1. Start **pmieconf** interactively (as the superuser).

    .. code-block:: bash

        pmieconf -f ${PCP_SYSCONF_DIR}/pmie/config.demo

        Updates will be made to ${PCP_SYSCONF_DIR}/pmie/config.demo

        pmieconf>

2. List a single rule:

    .. code-block:: bash

        pmieconf> list memory.swap_low
   
        rule: memory.swap_low  [Low free swap space]
        help: There is only threshold percent swap space remaining - the system
        may soon run out of virtual memory.  Reduce the number and size of
        the running programs or add more swap(1) space before it completely runs out.
        predicate =
           some_host (
                ( 100 * ( swap.free $hosts$ / swap.length $hosts$ ) )
                < $threshold$ && swap.length $hosts$ > 0 )
        vars: enabled = no
              threshold = 10%

        pmieconf>

3. List one rule variable:

    .. code-block:: bash

        pmieconf> list memory.swap_low threshold

        rule: memory.swap_low  [Low free swap space]
              threshold = 10%

        pmieconf>

4. Lower the threshold for the **memory.swap_low** rule, and also change the **pmie** sample interval affecting just this rule. The **delta** variable is special in that it is not associated with any particular rule; it has been defined as a global **pmieconf** variable.

    .. code-block:: bash

        pmieconf> modify memory.swap_low threshold 5

        pmieconf> modify memory.swap_low delta "1 sec"

        pmieconf>

5. Disable all of the rules except for the **memory.swap_low** rule so that we can see the effects of the change in isolation.

   This produces a relatively simple **pmie** configuration file:

    .. code-block:: bash

        pmieconf> disable all

        pmieconf> enable memory.swap_low

        pmieconf> status
            verbose:  off
            enabled rules:  1 of 35
            pmie configuration file:  ${PCP_SYSCONF_DIR}/pmie/config.demo
            pmie processes (PIDs) using this file:  (none found)

        pmieconf> quit

   .. note::
      We can also use the **status** command to verify that only one rule is enabled at the end of this step.

6. Run **pmie** with the new configuration file. Use a text editor to view the newly generated **pmie** configuration file (``${PCP_SYSCONF_DIR}/pmie/config.demo``), and then run the command:

   .. code-block:: bash

        pmie -T "1.5 sec" -v -l ${HOME}/demo.log ${PCP_SYSCONF_DIR}/pmie/config.demo
        memory.swap_low: false

        memory.swap_low: false

        cat ${HOME}/demo.log
        Log for pmie on venus started Mon Jun 21 16:26:06 2012

        pmie: PID = 21847, default host = venus

        [Mon Jun 21 16:26:07] pmie(21847) Info: evaluator exiting

        Log finished Mon Jun 21 16:26:07 2012

7. Notice that both of the **pmieconf** files used in the previous step are simple text files, as described in the **pmieconf(5)** man page:

   .. code-block:: bash

        file ${PCP_SYSCONF_DIR}/pmie/config.demo
        ${PCP_SYSCONF_DIR}/pmie/config.demo:  PCP pmie config (V.1)
        file ${PCP_VAR_DIR}/config/pmieconf/memory/swap_low
        ${PCP_VAR_DIR}/config/pmieconf/memory/swap_low:   PCP pmieconf rules (V.1)
