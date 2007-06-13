/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

#include <qmessagebox.h>
#include "qcolorpicker.h"
#include "hostdialog.h"
#include "source.h"
#include "chart.h"
#include "tab.h"
#include "main.h"

void ChartDialog::init()
{
    chartListSelected = FALSE;
    availableListSelected = FALSE;
    chartListSingleSelected = NULL;
    availableListSingleSelected = NULL;
    chartMetricsListView->header()->hide();
    availableMetricsListView->header()->hide();
    connect(chartMetricsListView, SIGNAL(selectionChanged()),
		this, SLOT(chartMetricsSelectionChanged()));
    connect(availableMetricsListView, SIGNAL(selectionChanged()),
		this, SLOT(availableMetricsSelectionChanged()));

    curCol = qRgb( -1, -1, -1 );
    QColIntValidator *val256 = new QColIntValidator(0, 255, this);
    QColIntValidator *val360 = new QColIntValidator(0, 360, this);

    hEd->setValidator(val360);
    sEd->setValidator(val256);
    vEd->setValidator(val256);
    rEd->setValidator(val256);
    gEd->setValidator(val256);
    bEd->setValidator(val256);

    connect(hEd, SIGNAL(textChanged(const QString&)), this, SLOT(hsvEd()));
    connect(sEd, SIGNAL(textChanged(const QString&)), this, SLOT(hsvEd()));
    connect(vEd, SIGNAL(textChanged(const QString&)), this, SLOT(hsvEd()));
    connect(rEd, SIGNAL(textChanged(const QString&)), this, SLOT(rgbEd()));
    connect(gEd, SIGNAL(textChanged(const QString&)), this, SLOT(rgbEd()));
    connect(bEd, SIGNAL(textChanged(const QString&)), this, SLOT(rgbEd()));

    connect(applyColorLabel,
	SIGNAL(colorDropped(QRgb)), this, SIGNAL(newCol(QRgb)));
    connect(applyColorLabel,
	SIGNAL(colorDropped(QRgb)), this, SLOT(setRgb(QRgb)));
    connect(colorPicker,
	SIGNAL(newCol(int,int)), luminancePicker, SLOT(setCol(int,int)));
    connect(luminancePicker,
	SIGNAL(newHsv(int,int,int)), this, SLOT(newHsv(int,int,int)));
    connect(this,
	SIGNAL(newCol(QRgb)), this, SLOT(newColorTypedIn(QRgb)));
}

void ChartDialog::reset(Chart *chart, int style)
{
    _chart = chart;
    if (!chart) {	// New
	tabWidget->setCurrentPage(1);
	chartMetricsListView->clear();
    }
    else {		// Edit
	setCaption(tr("Edit Chart"));
	tabWidget->setCurrentPage(0);
	chart->setupListView(chartMetricsListView);
    }
    if ((archiveMode = activeTab->isArchiveMode()) == TRUE) {
	QToolTip::add(sourceButton, tr("Add archives"));
	sourceButton->setPixmap(QPixmap::fromMimeSource("archive.png"));
    }
    else {
	QToolTip::add(sourceButton, tr("Add a host"));
	sourceButton->setPixmap(QPixmap::fromMimeSource("computer.png"));
    }
    titleLineEdit->setText(tr(""));
    typeComboBox->setCurrentItem(style);
    legendOn->setChecked(TRUE);
    legendOff->setChecked(FALSE);
    activeSources->setupListView(availableMetricsListView);
    _yAxisMin = yAxisMinimum->value();
    _yAxisMax = yAxisMaximum->value();

    chartListSelected = FALSE;
    availableListSelected = FALSE;
    chartListSingleSelected = NULL;
    availableListSingleSelected = NULL;
    enableUI();
}

void ChartDialog::enableUI()
{
    // TODO: if Utilisation mode, set Y-axis to 0-100 and disable change?

    // TODO: if (no) entries in chartMetricList, enable/disable OK button

    chartMetricLineEdit->setText(chartListSingleSelected ?
	((NameSpace *)chartListSingleSelected)->metricName() : tr(""));
    availableMetricLineEdit->setText(availableListSingleSelected ?
	((NameSpace *)availableListSingleSelected)->metricName() : tr(""));
    metricInfoButton->setEnabled(	// there can be only one
	(availableListSingleSelected && !chartListSingleSelected) ||
	(!availableListSingleSelected && chartListSingleSelected));
    metricDeleteButton->setEnabled(chartListSelected);
    metricAddButton->setEnabled(availableListSelected);

    revertColorButton->setEnabled(chartListSingleSelected != NULL);
    applyColorButton->setEnabled(chartListSingleSelected != NULL);
    if (chartListSingleSelected != NULL) {
	NameSpace *n = (NameSpace *)chartListSingleSelected;
	revertColorLabel->setPaletteBackgroundColor(n->originalColor());
	setCurrentColor(n->currentColor().rgb());
    }
    else {
	revertColorLabel->setPaletteBackgroundColor(QColor(0xff, 0xff, 0xff));
	setCurrentColor(QColor(0x00, 0x00, 0x00).rgb());
    }
}

Chart *ChartDialog::chart()
{
    return _chart;
}

void ChartDialog::chartMetricsSelectionChanged()
{
    QListViewItemIterator iterator(chartMetricsListView,
					QListViewItemIterator::Selected);
    chartListSingleSelected = iterator.current();
    if ((chartListSelected = (chartListSingleSelected != NULL)))
	if ((++iterator).current() != NULL)
	    chartListSingleSelected = NULL;	// multiple selections
    enableUI();
}

void ChartDialog::availableMetricsSelectionChanged()
{
    QListViewItemIterator iterator(availableMetricsListView,
					QListViewItemIterator::Selected);
    availableListSingleSelected = iterator.current();
    if ((availableListSelected = (availableListSingleSelected != NULL)))
	if ((++iterator).current() != NULL)
	    availableListSingleSelected = NULL;	// multiple selections
    enableUI();
}

void ChartDialog::metricInfoButtonClicked()
{
    NameSpace *ns;

    ns = (NameSpace *) (chartListSingleSelected ?
			chartListSingleSelected : availableListSingleSelected);
    kmchart->metricInfo(ns->sourceName(), ns->metricName(), ns->instanceName(),
			ns->isArchiveMode());
}

void ChartDialog::metricDeleteButtonClicked()
{
    NameSpace *name;
    QListViewItemIterator iterator(chartMetricsListView,
					QListViewItemIterator::Selected);
    for (; iterator.current(); ++iterator) {
        name = (NameSpace *)iterator.current();
	name->removeFromList(chartMetricsListView);
    }
}

void ChartDialog::metricAddButtonClicked()
{
    NameSpace *name;
    QPtrList<NameSpace> list;
    QListViewItemIterator iterator(availableMetricsListView,
					QListViewItemIterator::Selected);
    for (; iterator.current(); ++iterator)
        list.append((NameSpace *)iterator.current());
    availableMetricsListView->clearSelection();
    chartMetricsListView->clearSelection();	// selection(s) made below
    for (name = list.first(); name; name = list.next())
	name->addToList(chartMetricsListView);
}

void ChartDialog::sourceButtonClicked()
{
    int sts;

    if (archiveMode) {
	ArchiveDialog *a = new ArchiveDialog(this);
	QStringList al;

	if (a->exec() == QDialog::Accepted)
	    al = a->selectedFiles();
	for (QStringList::Iterator it = al.begin(); it != al.end(); ++it) {
	    QString ar = (*it).ascii();
	    if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, ar.ascii())) < 0) {
		ar.prepend(tr("Cannot open PCP archive: "));
		ar.append(tr("\n"));
		ar.append(tr(pmErrStr(sts)));
		QMessageBox::warning(this, pmProgname, ar,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    QMessageBox::NoButton, QMessageBox::NoButton);
	    } else {
		archiveSources->add(archiveGroup->which());
		archiveSources->setupListView(availableMetricsListView);
		archiveGroup->updateBounds();
	    }
	}
	delete a;
    } else {
	HostDialog *h = new HostDialog(this);

	if (h->exec() == QDialog::Accepted) {
	    QString proxy = h->proxyLineEdit->text().stripWhiteSpace();
	    if (proxy.isEmpty())
		unsetenv("PMPROXY_HOST");
	    else
		setenv("PMPROXY_HOST", proxy.ascii(), 1);
	    QString host = h->hostLineEdit->text().stripWhiteSpace();
	    if ((sts = liveGroup->use(PM_CONTEXT_HOST, host.ascii())) < 0) {
		host.prepend(tr("Cannot connect to host: "));
		host.append(tr("\n"));
		if (!proxy.isEmpty()) {
			host.append(tr(" proxy: "));
			host.append(proxy);
			host.append("\n");
		}
		host.append(tr(pmErrStr(sts)));
		QMessageBox::warning(this, pmProgname, host,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    QMessageBox::NoButton, QMessageBox::NoButton);
	    } else {
		liveSources->add(liveGroup->which());
		liveSources->setupListView(availableMetricsListView);
	    }
	}
	delete h;
    }
}

int ChartDialog::style(void)
{
    return typeComboBox->currentItem();
}

QString ChartDialog::title(void)
{
    return titleLineEdit->text();
}

bool ChartDialog::legend(void)
{
    return legendOn->isChecked();
}

void ChartDialog::legendOnClicked()
{
    legendOn->setChecked(TRUE);
    legendOff->setChecked(FALSE);
}

void ChartDialog::legendOffClicked()
{
    legendOn->setChecked(FALSE);
    legendOff->setChecked(TRUE);
}

void ChartDialog::scale(bool *autoscale, double *yaxismin, double *yaxismax)
{
    *autoscale = autoScaleOn->isChecked();
    *yaxismin = _yAxisMin;
    *yaxismax = _yAxisMax;
}

void ChartDialog::setScale(bool autoscale, double yaxismin, double yaxismax)
{
    autoScaleOn->setChecked(autoscale);
    autoScaleOff->setChecked(!autoscale);
    _yAxisMin = yaxismin;
    _yAxisMax = yaxismax;
}

void ChartDialog::autoScaleOnClicked()
{
    autoScaleOn->setChecked(TRUE);
    autoScaleOff->setChecked(FALSE);
    minTextLabel->setEnabled(FALSE);
    maxTextLabel->setEnabled(FALSE);
    yAxisMinimum->setEnabled(FALSE);
    yAxisMaximum->setEnabled(FALSE);
}

void ChartDialog::autoScaleOffClicked()
{
    autoScaleOn->setChecked(FALSE);
    autoScaleOff->setChecked(TRUE);
    minTextLabel->setEnabled(TRUE);
    maxTextLabel->setEnabled(TRUE);
    yAxisMinimum->setEnabled(TRUE);
    yAxisMaximum->setEnabled(TRUE);
}

void ChartDialog::yAxisMinimumValueChanged(double value)
{
    if (_yAxisMin != value) {
	_yAxisMin = value;
	if (_yAxisMin > _yAxisMax)
	    yAxisMaximum->setValue(value);
    }
}

void ChartDialog::yAxisMaximumValueChanged(double value)
{
    if (_yAxisMax != value) {
	_yAxisMax = value;
	if (_yAxisMax < _yAxisMin)
	    yAxisMinimum->setValue(value);
    }	
}

// Sets all widgets to display h,s,v
void ChartDialog::newHsv(int h, int s, int v)
{
    setHsv(h, s, v);
    colorPicker->setCol(h, s);
    luminancePicker->setCol(h, s, v);
}

// Sets all widgets to display rgb
void ChartDialog::setCurrentColor(QRgb rgb)
{
    setRgb(rgb);
    newColorTypedIn(rgb);
}

// Sets all widgets exept cs to display rgb
void ChartDialog::newColorTypedIn(QRgb rgb)
{
    int h, s, v;
    rgb2hsv(rgb, h, s, v );
    colorPicker->setCol( h, s );
    luminancePicker->setCol( h, s, v);
}

void ChartDialog::setRgb(QRgb rgb)
{
    curCol = rgb;
    rgb2hsv(currentColor(), hue, sat, val);
    hEd->setNum(hue);
    sEd->setNum(sat);
    vEd->setNum(val);
    rEd->setNum(qRed(currentColor()));
    gEd->setNum(qGreen(currentColor()));
    bEd->setNum(qBlue(currentColor()));
    showCurrentColor();
}

void ChartDialog::setHsv(int h, int s, int v)
{
    hue = h; sat = s; val = v;
    curCol = QColor(hue, sat, val, QColor::Hsv).rgb();
    hEd->setNum(hue);
    sEd->setNum(sat);
    vEd->setNum(val);
    rEd->setNum(qRed(currentColor()));
    gEd->setNum(qGreen(currentColor()));
    bEd->setNum(qBlue(currentColor()));
    showCurrentColor();
}

QRgb ChartDialog::currentColor()
{
    return curCol;
}

void ChartDialog::rgbEd()
{
    curCol = qRgb(rEd->val(), gEd->val(), bEd->val());
    rgb2hsv(currentColor(), hue, sat, val);
    hEd->setNum(hue);
    sEd->setNum(sat);
    vEd->setNum(val);
    showCurrentColor();
    emit newCol(currentColor());
}

void ChartDialog::hsvEd()
{
    hue = hEd->val();
    sat = sEd->val();
    val = vEd->val();
    curCol = QColor(hue, sat, val, QColor::Hsv).rgb();
    rEd->setNum(qRed(currentColor()));
    gEd->setNum(qGreen(currentColor()));
    bEd->setNum(qBlue(currentColor()));
    showCurrentColor();
    emit newCol(currentColor());
}

void ChartDialog::showCurrentColor()
{
    applyColorLabel->setColor(currentColor());
    applyColorLabel->repaint(FALSE);
}

void ChartDialog::applyColorButtonClicked()
{
    NameSpace *ns = (NameSpace *)chartListSingleSelected;
    ns->setCurrentColor(currentColor(), chartMetricsListView);
}

void ChartDialog::revertColorButtonClicked()
{
    NameSpace *ns = (NameSpace *)chartListSingleSelected;
    ns->setCurrentColor(ns->originalColor(), NULL);
}

void ChartDialog::setupChartPlots(Chart *cp)
{
    NameSpace *n;

    // First iterate over the current Charts metrics, removing any
    // that are no longer in the chartMetricsListView.  This is a
    // no-op in the createChart case, of course.

    int m;
    int nplots = cp->numPlot();	// Use a copy as we change it in the loop body
    for (m = 0; m < nplots; m++) {
	QListViewItemIterator it(chartMetricsListView,
				    QListViewItemIterator::Selectable);
	for (; it.current(); ++it) {
	    n = (NameSpace *)it.current();
	    if (matchChartPlot(cp, n, m))
		break;
	}
	if (it.current() == NULL)
	    deleteChartPlot(cp, m);
    }

    // Second step is to iterate over all the chartMetricsListView
    // entries, and either create new plots or edit existing ones.

    QListViewItemIterator iterator(chartMetricsListView,
				    QListViewItemIterator::Selectable);
    for (; iterator.current(); ++iterator) {
	n = (NameSpace *)iterator.current();
	if (existsChartPlot(cp, n, &m))
	    changeChartPlot(cp, n, m);
	else
	    createChartPlot(cp, n);
    }
}

bool ChartDialog::setupChartPlotsShortcut(Chart *cp)
{
    // This "shortcut" is used in the New Chart case - for speed in
    // creating new charts (a common operation), we allow the user
    // to bypass the step of moving plots from the Available Metrics
    // list to the Chart Metrics list.
    // IOW, if the Chart Metrics list is empty, but we do find one
    // or more Available Metrics selections, create a chart with them.
    // 
    // Return value indicates whether New Chart creation process is
    // complete at the end, or whether we need to continue on with
    // populating the new chart with Chart Metrics list plots.

    if (chartMetricsListView->childCount() > 0)
	return FALSE;	// go do regular creation paths

    int count;
    QListViewItemIterator iterator(availableMetricsListView,
				   QListViewItemIterator::Selected);
    for (count = 0; iterator.current(); ++iterator, count++) {
	NameSpace *n = (NameSpace *)iterator.current();
	QColor c = Chart::defaultColor(-1);
	n->setCurrentColor(c, NULL);
	createChartPlot(cp, n);
    }
    if (count == 0) {
	QString msg;
	msg.append(tr("No metrics were selected.\n"));
	msg.append(tr("Cannot create a new Chart\n"));
	QMessageBox::warning(this, pmProgname, msg,
		    QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		    QMessageBox::NoButton, QMessageBox::NoButton);
    }
    return TRUE;	// either way, we're finished now
}

bool ChartDialog::matchChartPlot(Chart *cp, NameSpace *name, int m)
{
    // compare: name, source, proxy
    if (strcmp(cp->metricName(m)->ptr(), name->metricName().ascii()) != 0)
	return FALSE;
    if (cp->metricContext(m) != name->metricContext())
	return FALSE;
    // TODO: proxy support needed here (string compare on proxy name)
    return TRUE;
}

bool ChartDialog::existsChartPlot(Chart *cp, NameSpace *name, int *m)
{
    int i;

    for (i = 0; i < cp->numPlot(); i++) {
	if (matchChartPlot(cp, name, i)) {
	    *m = i;
	    return TRUE;
	}
    }
    *m = -1;
    return FALSE;
}

void ChartDialog::changeChartPlot(Chart *cp, NameSpace *name, int m)
{
    cp->setColor(m, name->currentColor());
    // TODO: support for plot legend labels (at least preserve 'em!)
}

void ChartDialog::createChartPlot(Chart *cp, NameSpace *name)
{
    pmMetricSpec pms;

    pms.isarch = name->isArchiveMode();
    pms.source = strdup(name->sourceName().ascii());  // TODO: null, leak later?
    pms.metric = strdup(name->metricName().ascii());  // TODO: null, leak later?
    if (name->isInst()) {
	pms.ninst = 1;
	pms.inst[0] = strdup(name->instanceName().ascii());
    }
    else {
	pms.ninst = 0;
	pms.inst[0] = NULL;
    }
    int m = cp->addPlot(&pms, NULL);	// TODO: legend label support here
    cp->setColor(m, name->currentColor());
}

void ChartDialog::deleteChartPlot(Chart *cp, int m)
{
    cp->delPlot(m);
}
