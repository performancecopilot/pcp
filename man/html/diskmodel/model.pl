#
# Script generating a summary spreadsheet from production data
#
use strict;
use warnings;
use PCP::LogSummary;
use Spreadsheet::WriteExcel;

my $workbook = Spreadsheet::WriteExcel->new('model.xls');
my $naslog = "nasdata";	# PCP archive for NAS host
my $dblog = "dbdata";	# PCP archive for database host

# Setup some spreadsheet metadata - fonts, colors, etc
#
my $heading = $workbook->add_format();
$heading->set_bold();
$heading->set_italic();
my $subheading = $workbook->add_format();
$subheading->set_italic();
$subheading->set_bg_color('silver');
my $centercolumn = $workbook->add_format();
$centercolumn->set_align('center');
my $centerboldcolumn = $workbook->add_format();
$centerboldcolumn->set_align('center');
$centerboldcolumn->set_bold();

# Create a worksheet, configure a few columns
#
my $sheet = $workbook->add_worksheet();
$sheet->set_column('A:A', 28);	# metric names column
$sheet->set_column('B:B', 6);	# instances column
$sheet->set_column('C:C', 14);	# column for raw values
$sheet->set_column('D:D', 12);	# metrics units column
$sheet->set_column('E:E', 18);	# average size column
$sheet->set_column('F:F', 14);	# read/write ratio column

sub iops_ratio
{
    my ( $reads, $writes ) = @_;
    my $total = $writes + $reads;
    $writes /= $total;
    $writes *= 100;
    $writes = int($writes + 0.5);
    $reads /= $total;
    $reads *= 100;
    $reads = int($reads + 0.5);
    return "$reads:$writes";
}

# Write data - starting at row 0 and column 0, moving across and down
# 
my $row = 0;
$sheet->write($row, 0, 'NAS (NFS) Workload Modelling', $heading);
$row++;
$sheet->write($row, 0, 'Metrics', $subheading);
$sheet->write($row, 1, '', $subheading);
$sheet->write($row, 2, '', $subheading);
$sheet->write($row, 3, '', $subheading);
$sheet->write($row, 4, 'Average I/O Size', $subheading);
$sheet->write($row, 5, 'I/O Ratio', $subheading);

my @nas = ('disk.dev.read', 'disk.dev.read_bytes', 'disk.dev.write',
	   'disk.dev.write_bytes', 'disk.dev.avactive');
my $nasIO = PCP::LogSummary->new($naslog, \@nas, '@11:00', '@12:00');

foreach my $m ( @nas ) {
    my $device = 'sdi';
    my $metric = metric_instance($m, $device);
    $row++;
    $sheet->write($row, 0, $m);
    $sheet->write($row, 1, "[$device]");
    $sheet->write($row, 2, $$nasIO{$metric}{'average'}, $centercolumn);
    $sheet->write($row, 3, $$nasIO{$metric}{'units'});
    if ($m eq 'disk.dev.read') {
	my $thruput = metric_instance('disk.dev.read_bytes', $device);
	my $result = $$nasIO{$thruput}{'average'} / $$nasIO{$metric}{'average'};
	$sheet->write($row, 4, $result, $centerboldcolumn);

	my $writers = metric_instance('disk.dev.write', $device);
	my $writes = $$nasIO{$writers}{'average'};
	my $reads = $$nasIO{$metric}{'average'};
	$sheet->write($row, 5, iops_ratio($reads, $writes), $centerboldcolumn);
    }
    if ($m eq 'disk.dev.write') {
	my $thruput = metric_instance('disk.dev.write_bytes', $device);
	my $result = $$nasIO{$thruput}{'average'} / $$nasIO{$metric}{'average'};
	$sheet->write($row, 4, $result, $centerboldcolumn);
    }
}
$row += 2;

$sheet->write($row, 0, 'DB (Interactive) Workload Modelling', $heading);
$row++;
$sheet->write($row, 0, 'Metrics', $subheading);
$sheet->write($row, 1, '', $subheading);
$sheet->write($row, 2, '', $subheading);
$sheet->write($row, 3, '', $subheading);
$sheet->write($row, 4, 'Average I/O Size', $subheading);
$sheet->write($row, 5, 'I/O Ratio', $subheading);

my @db = ('disk.dev.read', 'disk.dev.read_bytes', 'disk.dev.write',
	  'disk.dev.write_bytes', 'disk.dev.queue_len', 'disk.dev.idle');
my $dbIO = PCP::LogSummary->new($dblog, \@db, '@11:10', '@11:55');

foreach my $m ( @db ) {
    my $device = 'F:';
    my $metric = metric_instance($m, $device);
    $row++;
    $sheet->write($row, 0, $m);
    $sheet->write($row, 1, "[$device]");
    $sheet->write($row, 2, $$dbIO{$metric}{'average'}, $centercolumn);
    $sheet->write($row, 3, $$dbIO{$metric}{'units'});
    if ($m eq 'disk.dev.read') {
	my $thruput = metric_instance('disk.dev.read_bytes', $device);
	my $result = $$dbIO{$thruput}{'average'} / $$dbIO{$metric}{'average'};
	$sheet->write($row, 4, $result, $centerboldcolumn);

	my $writers = metric_instance('disk.dev.write', $device);
	my $writes = $$dbIO{$writers}{'average'};
	my $reads = $$dbIO{$metric}{'average'};
	$sheet->write($row, 5, iops_ratio($reads, $writes), $centerboldcolumn);
    }
    if ($m eq 'disk.dev.write') {
	my $thruput = metric_instance('disk.dev.write_bytes', $device);
	my $result = $$dbIO{$thruput}{'average'} / $$dbIO{$metric}{'average'};
	$sheet->write($row, 4, $result, $centerboldcolumn);
    }
}
$row += 2;

$sheet->write($row, 0, 'DB (Business Intel) Workload Modelling', $heading);
$row++;
$sheet->write($row, 0, 'Metrics', $subheading);
$sheet->write($row, 1, '', $subheading);
$sheet->write($row, 2, '', $subheading);
$sheet->write($row, 3, '', $subheading);
$sheet->write($row, 4, 'Average I/O Size', $subheading);
$sheet->write($row, 5, 'I/O Ratio', $subheading);

my $dbBI = PCP::LogSummary->new($dblog, \@db, '@11:00', '@11:05');

foreach my $m ( @db ) {
    my $device = 'F:';
    my $metric = metric_instance($m, $device);
    $row++;
    $sheet->write($row, 0, $m);
    $sheet->write($row, 1, "[$device]");
    $sheet->write($row, 2, $$dbBI{$metric}{'average'}, $centercolumn);
    $sheet->write($row, 3, $$dbBI{$metric}{'units'});
    if ($m eq 'disk.dev.read') {
	my $thruput = metric_instance('disk.dev.read_bytes', $device);
	my $result = $$dbBI{$thruput}{'average'} / $$dbBI{$metric}{'average'};
	$sheet->write($row, 4, $result, $centerboldcolumn);
	my $writers = metric_instance('disk.dev.write', $device);
	my $writes = $$dbBI{$writers}{'average'};
	my $reads = $$dbBI{$metric}{'average'};
	$sheet->write($row, 5, iops_ratio($reads, $writes), $centerboldcolumn);
    }
    if ($m eq 'disk.dev.write') {
	my $thruput = metric_instance('disk.dev.write_bytes', $device);
	my $result = $$dbBI{$thruput}{'average'} / $$dbBI{$metric}{'average'};
	$sheet->write($row, 4, $result, $centerboldcolumn);
    }
}
