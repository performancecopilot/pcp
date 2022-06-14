//
// Test QmcMetric::formatValue routines
//

#include <QTextStream>
#include <qmc.h>
#include <qmc_context.h>
#include <qmc_metric.h>

QTextStream cerr(stderr);
QTextStream cout(stdout);

int
main(int argc, char *argv[])
{
    double	d;
    char	*endptr;

    pmSetProgname(argv[0]);
    if (argc != 2) {
	cerr << "Usage: " << pmGetProgname() << " double" << Qt::endl;
	exit(1);
	/*NOTREACHED*/
    }

    d = strtod(argv[1], &endptr);
    if (endptr != NULL && endptr[0] != '\0') {
	cerr << pmGetProgname() << ": argument \"" << argv[1] 
	     << "\" must be a double (\"" << endptr << "\")" << Qt::endl;
	exit(1);
	/*NOTREACHED*/
    }

    cout << QmcMetric::formatNumber(d) << Qt::endl;

    return 0;
}
