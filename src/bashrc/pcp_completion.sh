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
    pcp2elasticsearch)
        all_args="ahLKcCeVHGASTmOstRrIijJ4589nNvP0qQbByYgXxp"
        arg_regex="-[ahKceASTOstZiJ489NP0qQbByYgXxp]"
    ;;
    pcp2graphite)
        all_args="ahLKcCeVHGASTOstRrIijJ4589nNvP0qQbByYgpXEx"
        arg_regex="-[ahKceASTOstZiJ489NP0qQbByYgpXEx]"
    ;;
    pcp2influxdb)
        all_args="ahLKcCeVHGASTOstRrIijJ4589nNvP0qQbByYgxUEX"
        arg_regex="-[ahKceASTOstZiJ489NP0qQbByYgxUEX]"
    ;;
    pcp2json)
        all_args="ahLKcCeVHGASTmOstRrZzrIijJ4589nNvP0qQbByYFfxXE"
        arg_regex="-[ahKceASTOstZiJ489NP0qQbByYFf]"
    ;;
    pcp2spark)
        all_args="ahLKcCeVHGASTOstRrZzrIijJ4589nNvP0qQbByYgp"
        arg_regex="-[ahKceASTOstZiJ489NP0qQbByYgp]"
    ;;
    pcp2xlsx)
        all_args="ahLKcCeVHGASTmOstRrZzrIivP045qQbByYFf"
        arg_regex="-[ahKceASTOstZiP04qQbByYgFf]"
    ;;
    pcp2xml)
        all_args="ahLKcCeVHGASTOstRrZzrIijJ4589mnNvP0qQbByYFfXx"
        arg_regex="-[ahKceASTOstZiJ489NP0qQbByYgFf]"
    ;;
    pcp2zabbix)
        all_args="ahLKcCeVHGASTOstRrIijJ4589nNvP0qQbByYgpXExl"
        arg_regex="-[ahKceASTOstiJ489NP0qQbByYgpXEx]"
    ;;
    pmclient)
        all_args="AahnOPSsTtVZz"
        arg_regex="-[AahnOSsTtZz]"
    ;;
    pmdumplog)
        all_args="adehIiLlMmnrSsTtVvxZz"
        arg_regex="-[nSTvZ]"
    ;;
    pmdumptext)
        all_args="AaCcdFfGHhilMmNnOoPRrSstTUuVXwZz"
        arg_regex="-[AacdfhnOPRSsTtUwZ]"
    ;;
    pmevent)
        all_args="AadfghiKLnOprSsTtUVvwXxZz"
        arg_regex="-[AafhiKnOpSsTtUwxZ]"
    ;;
    pmfind)
        all_args="CmqrSstV"
        arg_regex="-[mst]"
    ;;
    pmie)
        all_args="AabCcdeFfhjlmnOPqSTtUVvWXxZz"
        arg_regex="-[AachljnOSTtUZ]"
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
        all_args="abcdFfhIKLlMmNnOsTtVvxZz"
        arg_regex="-[abchKNnOZ]"
    ;;
    pmjson)
        all_args="imopqyV"
        arg_regex="-[io]"
    ;;
    pmlc)
        all_args="ehinPpZz"
        arg_regex="-[hnpZ]"
    ;;
    pmlogcheck)
        all_args="lmnSTvwZz"
        arg_regex="-[nSTZ]"
    ;;
    pmlogctl)
        all_args="aCcfimNpV"
        arg_regex="-[Ccip]"
    ;;
    pmlogextract)
        all_args="cdfmSsTVvwxZz"
        arg_regex="-[cSsTVvZ]"
    ;;
    pmlogger)
        all_args="CcdHIhKLlmNnoPprsTtUuVvxy"
        arg_regex="-[cdHIhKlmnpsTtUVvx]"
    ;;
    pmloglabel)
        all_args="hLlpsVvZ"
        arg_regex="-[hpVZ]"
    ;;
    pmlogpaste)
        all_args="fhlmot"
        arg_regex="-[fhlmot]"
    ;;
    pmlogreduce)
        all_args="ASsTtvZz"
        arg_regex="-[ASsTtZ]"
    ;;
    pmlogsize)
        all_args="drvx"
        arg_regex="-[x]"
    ;;
    pmlogsummary)
        all_args="aBbFfHIilMmNnpSsTVvxyZz"
        arg_regex="-[BnpSTZ]"
    ;;
    pmprobe)
        all_args="abdfFhIiKLnOVvZz"
        arg_regex="-[abhKnOZ]"
    ;;
    pmrep)
        all_args="0123456789AaBbCcdEeFfGgHhIiJjKkLlmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        arg_regex="-[04689ABabcEeFfhiJKlNOoPQqSsTtWwXYyZ]"
    ;;
    pmseries)
        all_args="acdFghIiLlMmnpqSstVvwZ"
        arg_regex="-[cghpwZ]"
    ;;
    pmstat)
        all_args="AagHhLlnOPpSsTtVxZz"
        arg_regex="-[AaHhnOpSsTtZ]"
    ;;
    pmstore)
        all_args="FfhiKLnV"
        arg_regex="-[hiKn]"
    ;;
    pmval)
        all_args="AadfghiKLnOprSsTtUVvwXxZz"
        arg_regex="-[AafhiKnOpSsTtUwxZ]"
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
complete -F _pcp_complete -o default pcp2elasticsearch pcp2graphite pcp2influxdb pcp2json pcp2spark pcp2xlsx pcp2xml pcp2zabbix pmclient pmdumplog pmdumptext pmevent pmfind pmie pmie2col pmiectl pminfo pmjson pmlc pmlogcheck pmlogctl pmlogextract pmlogger pmloglabel pmlogpaste pmlogreduce pmlogsize pmlogsummary pmprobe pmrep pmseries pmstat pmstore pmval
