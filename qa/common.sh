. /etc/pcp.env

case `pwd`
in
    */qa)
    	export PATH=.:`cd ..; pwd`:$PATH
	;;
    *)
	echo "Warning: not in qa directory, I'm confused"
	pwd
	status=1
	exit
	;;
esac
