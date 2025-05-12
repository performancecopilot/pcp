_pcp_complete()
{
    # All supported arguments for a command
    local all_args=""
    # Regex for args expecting non-metric parameter
    local arg_regex=""

    local cmd=${COMP_WORDS[0]}
    local cur=${COMP_WORDS[$COMP_CWORD]}

    COMPREPLY=()

    # Register arguments
    case $cmd in
    pcp2arrow)
        all_args="ahLKcCDVASTOstrRijJ89nZzo"
        arg_regex="-[ahKcCDASTOstiJ89Zo]"
    ;;
    pcp2elasticsearch)
	all_args="ahLKcCeDVHGASTOstrRIijJ4589nNvmP0qbyQBYgxXp"
	arg_regex="-[ahKceDASTOstiJ489NP0qbyQBYgxXp]"
    ;;
    pcp2graphite)
	all_args="ahLKcCeDVHGASTOstrRIijJ4589nNvP0qbyQBYgpXEx"
	arg_regex="-[ahKceDASTOstiJ489NP0qbyQBYgpXEx]"
    ;;
    pcp2influxdb)
	all_args="ahLKcCeDVHGASTOstrRIijJ4589nNvP0qbyQBYgxUEX"
	arg_regex="-[ahKceDASTOstiJ489NP0qbyQBYgxUEX]"
    ;;
    pcp2json)
	all_args="ahLKcCeDVHGASTOstrRIijJ4589nNvmP0qbyQBYFfZzxXEopUu"
	arg_regex="-[ahKceDASTOstiJ489NP0qbyQBYFfZopUu]"
    ;;
    pcp2spark)
	all_args="ahLKcCeDVHGASTOstrRIijJ4589nNvmP0qbyQBYgp"
	arg_regex="-[ahKceDASTOstiJ489NP0qbyQBYgp]"
    ;;
    pcp2xlsx)
	all_args="ahLKcCeDVHGASTOstrRIi45vmP0qbyQBYFfZz"
	arg_regex="-[ahKceDASTOsti4P0qbyQBYFfZ]"
    ;;
    pcp2xml)
	all_args="ahLKcCeDVHGASTOstrRIijJ4589nNvmP0qbyQBYFfZzxX"
	arg_regex="-[ahKceDASTOstiJ489NP0qbyQBYFfZ]"
    ;;
    pcp2zabbix)
	all_args="ahLKcCeDVHGASTOstrRIijJ4589nNvP0qbyQBYgpXExl"
	arg_regex="-[ahKceDASTOstiJ489NP0qbyQBYgpXEx]"
    ;;
    pmclient)
	all_args="AaDhnOSsTtZzVP"
	arg_regex="-[AaDhnOSsTtZ]"
    ;;
    pmlogdump|pmdumplog)
	all_args="aDdehIilLmMnrSsTtVvxZz"
	arg_regex="-[DnSTvZ]"
    ;;
    pmdumptext)
	all_args="AaDhnOSsTtVZzcCdfFGHilmMNoPrRuUVwX"
	arg_regex="-[AaDhnOSsTtZcdfPRUw]"
    ;;
    pmevent)
	all_args="AaDghnOpSsTtVZzdfiKLrUvwxX"
	arg_regex="-[AaDhnOpSsTtZfiKUwx]"
    ;;
    pmfind)
	all_args="CDmrsStqV"
	arg_regex="-[Dmst]"
    ;;
    pmie)
	all_args="aAbcCdDefFhjlmnoOPqStTUvVWXxzZ"
	arg_regex="-[aAcDhjlmnoOStTUZ]"
    ;;
    pmie2col)
        all_args="dpw"
        arg_regex="-[dpw]"
    ;;
    pmiectl)
        all_args="aCcfimNpV"
        arg_regex="-[Ccip]"
    ;;
    pminfo)
	all_args="abcdDFfhIKlLMmNnOrstTvVxzZ"
	arg_regex="-[abcDhKNnOrZ]"
    ;;
    pmjson)
	all_args="DmiopqyV"
	arg_regex="-[Dio]"
    ;;
    pmlc)
	all_args="DehinPpzZ"
	arg_regex="-[DhnpZ]"
    ;;
    pmlogcheck)
	all_args="DlmnSTzvwZ"
	arg_regex="-[DnSTZ]"
    ;;
    pmlogctl)
        all_args="aCcfimNpV"
        arg_regex="-[Ccip]"
    ;;
    pmlogextract)
	all_args="cDdfmSsTVvwxZz"
	arg_regex="-[cDSsTVvZ]"
    ;;
    pmlogger)
	all_args="cCdDhHIlKLmNnopPrsTtuUvVxy"
	arg_regex="-[cdDhHIlKmnpsTtUvVx]"
    ;;
    pmloglabel)
	all_args="DhlLpsvVZ"
	arg_regex="-[DhpVZ]"
    ;;
    pmlogpaste)
        all_args="fhlmot"
        arg_regex="-[fhlmot]"
    ;;
    pmlogreduce)
	all_args="ADSsTtvZz"
	arg_regex="-[ADSsTtvZ]"
    ;;
    pmlogsize)
	all_args="dDrvx"
	arg_regex="-[Dx]"
    ;;
    pmlogsummary)
	all_args="abBDfFHiIlmMNnpsSTvVxyzZ"
	arg_regex="-[BDnpSTZ]"
    ;;
    pmprobe)
	all_args="abDdfhIiKLnFOVvZz"
	arg_regex="-[abDhKnOZ]"
    ;;
    pmrep)
	all_args="ahLKcCoFeDVHUGpASTOstZzdrRIijJ2345789nN6vmXWwP0lkxE1gfuqbyQBY"
	arg_regex="-[ahKcoFeDASTOstZiJ489N6XWwP0lEfqbyQBY]"
    ;;
    pmseries)
	all_args="acdDFghiIlLmMnqpsStvVwZ"
	arg_regex="-[cDghpwZ]"
    ;;
    pmstat)
	all_args="AaDghnOpSsTtVZzHLlPx"
	arg_regex="-[AaDhnOpSsTtZH]"
    ;;
    pmstore)
	all_args="DFfhKLinV"
	arg_regex="-[DhKin]"
    ;;
    pmval)
	all_args="AaDghnOpSsTtVZzdfiKLrUvwxX"
	arg_regex="-[AaDhnOpSsTtZfiKUwx]"
    ;;
    esac

    # Complete
    pytool=0
    [[ "pcp2elasticsearch pcp2graphite pcp2influxdb pcp2json pcp2spark pcp2xlsx pcp2xml pcp2zabbix pmrep" =~ $cmd ]] && pytool=1
    if [[ "$cur" == -* ]]; then
        # Arguments
        local comp=( $(echo $all_args | sed -e 's,.\{1\},-& ,g') )
        COMPREPLY=( $(compgen -W "${comp[*]}" -- "$cur") )
    elif [[ $pytool -eq 1 && ("$cur" == :* || ${COMP_WORDS[$((COMP_CWORD-1))]} == :) ]]; then
        # pmrep(1) style metricset
        local conf=""
        for i in $(seq 1 $COMP_CWORD); do
            if [[ "${COMP_WORDS[$i]}" == -c || "${COMP_WORDS[$i]}" == --config ]]; then
                conf="${COMP_WORDS[(($i+1))]}"
                [[ ! -e $conf ]] && COMPREPLY=("") && return
                break
            fi
        done
        if [[ -d $conf ]]; then
            if compgen -G "$conf/*.conf" > /dev/null; then
                conf=($conf/*.conf)
            fi
        elif [[ -z $conf ]]; then
            local defconfdir=$(grep ^PCP_SYSCONF_DIR= /etc/pcp.conf 2> /dev/null | cut -d= -f2)/$cmd
            for f in ./$cmd.conf $HOME/.$cmd.conf $HOME/.pcp/$cmd.conf $defconfdir/$cmd.conf; do
                [[ -f $f ]] && conf=($f) && break
            done
            if [[ -z $conf && $cmd == pmrep ]]; then
                if compgen -G "$defconfdir/*.conf" > /dev/null; then
                    conf=($defconfdir/*.conf)
                fi
            fi
        fi
        [[ -z $conf || -d $conf ]] && COMPREPLY=("") && return
        local sets=()
        for f in ${conf[@]}; do
            while read line; do
                if [[ $line == \[*\] && $line != \[global\] && $line != \[options\] ]]; then
                    local set=${line/[} ; set=${set/]}
                    sets+=($set)
                fi
            done < $f
        done
        [[ -z $sets ]] && COMPREPLY=("") && return
        [[ "$cur" == : ]] && cur=
        COMPREPLY=( $(compgen -W "${sets[*]}" -- "$cur") )
    elif [[ $cmd == pmseries && ! "${COMP_WORDS[$((COMP_CWORD-1))]}" =~ $arg_regex ]]; then
        # pmseries(1) metric names
        COMPREPLY=( $(compgen -W '$(command pmseries -m 2> /dev/null)' -- "$cur") )
    elif [[ ! "${COMP_WORDS[$((COMP_CWORD-1))]}" =~ $arg_regex ]]; then
        # Metric names
        if [[ ($cmd != pmlogsummary && $cmd != pmstat) || \
            ( $COMP_CWORD > 1 && ${COMP_WORDS[$((COMP_CWORD-1))]} != -* && ${COMP_WORDS[$((COMP_CWORD-2))]} != -* ) ]]; then
            COMPREPLY=( $(compgen -W '$(command pminfo 2> /dev/null)' -- "$cur") )
        fi
    fi
}
complete -F _pcp_complete -o default pcp2elasticsearch pcp2graphite pcp2influxdb pcp2json pcp2spark pcp2xlsx pcp2xml pcp2zabbix pmclient pmdumplog pmdumptext pmevent pmfind pmie pmie2col pmiectl pminfo pmjson pmlc pmlogcheck pmlogctl pmlogdump pmlogextract pmlogger pmloglabel pmlogpaste pmlogreduce pmlogsize pmlogsummary pmprobe pmrep pmseries pmstat pmstore pmval
