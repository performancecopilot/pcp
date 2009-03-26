# A demo script which interfaces the LogSummary Perl module to the
# WriteExcel module, with some real data, to show what it can do.
# Author: Nathan Scott <nathans@debian.org>
# Note: No #! line, as this pulls in an external dependency that
#       we really don't want in packaging tools like rpm.

use strict;
use warnings;
use PCP::LogSummary;
use Spreadsheet::WriteExcel;

my @app = ('aconex.response_time.samples', 'aconex.response_time.adjavg');
my @db = ('kernel.all.cpu.user', 'kernel.all.cpu.sys', 'kernel.all.cpu.intr');
my @dbdisk = ('disk.dev.idle', 'disk.dev.read_bytes');

my ( $dblog1, $dblog2 ) = ('t/db/20081125', 't/db/20081126');
my $dbdisk_before = PCP::LogSummary->new($dblog1, \@dbdisk);
my $dbdisk_after  = PCP::LogSummary->new($dblog2, \@dbdisk);
my $dbcpu_before = PCP::LogSummary->new($dblog1, \@db);
my $dbcpu_after  = PCP::LogSummary->new($dblog2, \@db);

my ( $applog1, $applog2 ) = ('t/app/20081125', 't/app/20081126');
my $app_before = PCP::LogSummary->new($applog1, \@app);
my $app_after  = PCP::LogSummary->new($applog2, \@app);

my $workbook = Spreadsheet::WriteExcel->new('new-dxb-dbserver.xls');

my $heading = $workbook->add_format();
$heading->set_bold();
$heading->set_italic();
my $subheading = $workbook->add_format();
$subheading->set_italic();
$subheading->set_bg_color('silver');
my $unitscolumn = $workbook->add_format();
$unitscolumn->set_align('center');

my $sheet = $workbook->add_worksheet();
my ( $precol, $postcol, $units ) = ( 1, 2, 3 );
$sheet->set_column('A:A', 32);	# metric names column
$sheet->set_column('B:B', 14);	# column for "Before" values
$sheet->set_column('C:C', 14);	# column for "After" values
$sheet->set_column('D:D', 12);	# metrics units column

my $row = 0;
$sheet->write($row, 0, 'Dubai Database Storage Upgrade', $heading);
$row = 2;
$sheet->write($row, 0, 'Metrics', $subheading);
$sheet->write($row, $precol, 'Before', $subheading);
$sheet->write($row, $postcol, 'After', $subheading);
$sheet->write($row, $units, 'Units', $subheading);

foreach my $m ( @app ) {
    my $metric = metric_instance($m, 'dxb');
    $row++;
    $sheet->write($row, 0, $metric);
    $sheet->write($row, $precol, $$app_before{$metric}{'average'});
    $sheet->write($row, $postcol, $$app_after{$metric}{'average'});
    $sheet->write($row, $units, $$app_after{$metric}{'units'}, $unitscolumn);
}
foreach my $m ( @dbdisk ) {
    my $metric = metric_instance($m, 'G:');	# Windows drive letter
    $row++;
    $sheet->write($row, 0, $metric);
    $sheet->write($row, $precol, $$dbdisk_before{$metric}{'average'});
    $sheet->write($row, $postcol, $$dbdisk_after{$metric}{'average'});
    $sheet->write($row, $units, $$dbdisk_after{$metric}{'units'}, $unitscolumn);
}

# Report CPU metrics as a single utilisation value
{
    my $syscpu1 = $$dbcpu_before{'kernel.all.cpu.sys'};
    my $intcpu1 = $$dbcpu_before{'kernel.all.cpu.intr'};
    my $usrcpu1 = $$dbcpu_before{'kernel.all.cpu.user'};
    my $syscpu2 = $$dbcpu_after{'kernel.all.cpu.sys'};
    my $intcpu2 = $$dbcpu_after{'kernel.all.cpu.intr'};
    my $usrcpu2 = $$dbcpu_after{'kernel.all.cpu.user'};
    my $ncpu = 4;

    my $cpu_before = $ncpu * ( $$syscpu1{'average'} +
		$$intcpu1{'average'} + $$usrcpu1{'average'} );
    my $cpu_after  = $ncpu * ( $$syscpu2{'average'} +
		$$intcpu2{'average'} + $$usrcpu2{'average'} );

    $row++;
    $sheet->write($row, 0, 'kernel.all.cpu');
    $sheet->write($row, $precol, $cpu_before * 100.0);
    $sheet->write($row, $postcol, $cpu_after * 100.0);
    $sheet->write($row, $units, 'percent', $unitscolumn);
}
