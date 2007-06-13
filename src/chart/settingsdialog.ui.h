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

#include "main.h"
#include "qcolordialog.h"

int SettingsDialog::defaultColorArray(QPushButton ***array)
{
    static QPushButton *buttons[] = {
	defaultColorsPushButton1,  defaultColorsPushButton2,
	defaultColorsPushButton3,  defaultColorsPushButton4,
	defaultColorsPushButton5,  defaultColorsPushButton6,
	defaultColorsPushButton7,  defaultColorsPushButton8,
	defaultColorsPushButton9,  defaultColorsPushButton10,
	defaultColorsPushButton11, defaultColorsPushButton12,
	defaultColorsPushButton13, defaultColorsPushButton14,
	defaultColorsPushButton15, defaultColorsPushButton16,
	defaultColorsPushButton17, defaultColorsPushButton18,
    };
    *array = &buttons[0];
    return sizeof(buttons) / sizeof(buttons[0]);
}

void SettingsDialog::reset()
{
    QPushButton **buttons;
    QColor white("white");
    int i, defaultColorCount, userColorCount;

    _localVisible = _localTotal = 0;
    settings.sampleHistoryModified = FALSE;
    settings.visibleHistoryModified = FALSE;
    settings.defaultColorsModified = FALSE;
    settings.chartBackgroundModified = FALSE;
    settings.chartHighlightModified = FALSE;
    settings.styleModified = FALSE;

    visibleCounter->setValue(settings.visibleHistory);
    visibleSlider->setValue(settings.visibleHistory);
    visibleSlider->setRange(1, MAXIMUM_VISIBLE_POINTS);
    totalCounter->setValue(settings.sampleHistory);
    totalSlider->setValue(settings.sampleHistory);
    totalSlider->setRange(1, MAXIMUM_TOTAL_POINTS);

    chartBackgroundPushButton->setPaletteBackgroundColor(
						settings.chartBackground);
    chartHighlightPushButton->setPaletteBackgroundColor(
						settings.chartHighlight);
    defaultColorCount = settings.defaultColorNames.count();
    userColorCount = defaultColorArray(&buttons);
    for (i = 0; i < defaultColorCount; i++)
	buttons[i]->setPaletteBackgroundColor(settings.defaultColors[i]);
    for (; i < userColorCount; i++)
	buttons[i]->setPaletteBackgroundColor(white);

    _savedStyle = settings.style;
    _savedStyleName = settings.styleName;
    motifStyleRadioButton->setChecked(settings.styleName == "Motif");
    windowsStyleRadioButton->setChecked(settings.styleName == "Windows");
    cdeStyleRadioButton->setChecked(settings.styleName == "cde");
    motifPlusStyleRadioButton->setChecked(settings.styleName == "MotifPlus");
    platinumStyleRadioButton->setChecked(settings.styleName == "Platinum");
    sgiStyleRadioButton->setChecked(settings.styleName == "SGI");
    macintoshRadioButton->setChecked(settings.styleName == "Macintosh");
    aquaRadioButton->setChecked(settings.styleName == "Aqua");
    windowsXPRadioButton->setChecked(settings.styleName == "WindowsXP");
}

void SettingsDialog::revert()
{
    // Style is special - we make immediate changes for instant feedback
    // TODO: this doesn't work.  Problem is the default case where we do
    // not have a known style name to fallback to...
    if (settings.styleModified) {
	if (_savedStyleName == QString::null)
	    _savedStyleName = tr("");
	settings.styleName = _savedStyleName;
	settings.style = QApplication::setStyle(_savedStyleName);
	kmtime->styleTimeControl((char *)_savedStyleName.ascii());
    }
}

void SettingsDialog::flush()
{
    if (settings.visibleHistoryModified)
	settings.visibleHistory = (int)_localVisible;
    if (settings.sampleHistoryModified)
	settings.sampleHistory = (int)_localTotal;

    if (settings.defaultColorsModified) {
	QPushButton **buttons;
	QStringList colorNames;
	QValueList<QColor> colors;
	QColor c, white("white");
	int i, userColorCount;

	userColorCount = defaultColorArray(&buttons);
	for (i = 0; i < userColorCount; i++) {
	    QColor c = buttons[i]->paletteBackgroundColor();
	    if (c == white)
		continue;
	    colors.append(c);
	    colorNames.append(c.name());
	}

	settings.defaultColors = colors;
	settings.defaultColorNames = colorNames;
    }

    if (settings.chartBackgroundModified) {
	settings.chartBackground =
			chartBackgroundPushButton->paletteBackgroundColor();
	settings.chartBackgroundName = settings.chartBackground.name();
    }
    if (settings.chartHighlightModified) {
	settings.chartHighlight =
			chartHighlightPushButton->paletteBackgroundColor();
	settings.chartHighlightName = settings.chartHighlight.name();
    }
}

void SettingsDialog::visibleValueChanged(double value)
{
    if (value != _localVisible) {
	_localVisible = (double)(int)value;
	displayVisibleCounter();
	displayVisibleSlider();
	if (_localVisible > _localTotal)
	    totalSlider->setValue(value);
	settings.visibleHistoryModified = TRUE;
    }
}

void SettingsDialog::totalValueChanged(double value)
{
    if (value != _localTotal) {
	_localTotal = (double)(int)value;
	displayTotalCounter();
	displayTotalSlider();
	if (_localVisible > _localTotal)
	    visibleSlider->setValue(value);
	settings.sampleHistoryModified = TRUE;
    }
}

void SettingsDialog::displayTotalSlider()
{
    totalSlider->blockSignals(TRUE);
    totalSlider->setValue(_localTotal);
    totalSlider->blockSignals(FALSE);
}

void SettingsDialog::displayVisibleSlider()
{
    visibleSlider->blockSignals(TRUE);
    visibleSlider->setValue(_localVisible);
    visibleSlider->blockSignals(FALSE);
}

void SettingsDialog::displayTotalCounter()
{
    totalCounter->blockSignals(TRUE);
    totalCounter->setValue(_localTotal);
    totalCounter->blockSignals(FALSE);
}

void SettingsDialog::displayVisibleCounter()
{
    visibleCounter->blockSignals(TRUE);
    visibleCounter->setValue(_localVisible);
    visibleCounter->blockSignals(FALSE);
}

void SettingsDialog::chartHighlightPushButtonClicked()
{
    QColor newColor = QColorDialog::getColor(
		chartHighlightPushButton->paletteBackgroundColor());
    if (newColor.isValid()) {
	chartHighlightPushButton->setPaletteBackgroundColor(newColor);
	settings.chartHighlightModified = TRUE;
    }
}

void SettingsDialog::chartBackgroundPushButtonClicked()
{
    QColor newColor = QColorDialog::getColor(
			chartBackgroundPushButton->paletteBackgroundColor());
    if (newColor.isValid()) {
	chartBackgroundPushButton->setPaletteBackgroundColor(newColor);
	settings.chartBackgroundModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton1Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton1->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton1->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton2Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton2->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton2->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton3Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton3->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton3->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton4Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton4->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton4->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton5Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton5->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton5->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton6Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton6->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton6->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton7Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton7->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton7->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton8Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton8->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton8->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton9Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton9->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton9->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton10Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton10->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton10->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton11Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton11->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton11->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton12Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton12->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton12->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton13Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton13->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton13->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton14Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton14->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton14->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton15Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton15->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton15->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton16Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton16->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton16->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton17Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton17->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton17->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::defaultColorsPushButton18Clicked()
{
    QColor newColor = QColorDialog::getColor(
			defaultColorsPushButton18->paletteBackgroundColor());
    if (newColor.isValid()) {
	defaultColorsPushButton18->setPaletteBackgroundColor(newColor);
	settings.defaultColorsModified = TRUE;
    }
}

void SettingsDialog::windowsStyleRadioButtonClicked()
{
    static char windows[] = "Windows";

    windowsStyleRadioButton->setChecked(TRUE);
    motifStyleRadioButton->setChecked(FALSE);
    cdeStyleRadioButton->setChecked(FALSE);
    motifPlusStyleRadioButton->setChecked(FALSE);
    platinumStyleRadioButton->setChecked(FALSE);
    sgiStyleRadioButton->setChecked(FALSE);
    macintoshRadioButton->setChecked(FALSE);
    aquaRadioButton->setChecked(FALSE);
    windowsXPRadioButton->setChecked(FALSE);

    settings.styleName = tr(windows);
    settings.style = QApplication::setStyle(settings.styleName);
    kmtime->styleTimeControl(windows);
    settings.styleModified = TRUE;
}

void SettingsDialog::motifStyleRadioButtonClicked()
{
    static char motif[] = "Motif";

    windowsStyleRadioButton->setChecked(FALSE);
    motifStyleRadioButton->setChecked(TRUE);
    cdeStyleRadioButton->setChecked(FALSE);
    motifPlusStyleRadioButton->setChecked(FALSE);
    platinumStyleRadioButton->setChecked(FALSE);
    sgiStyleRadioButton->setChecked(FALSE);
    macintoshRadioButton->setChecked(FALSE);
    aquaRadioButton->setChecked(FALSE);
    windowsXPRadioButton->setChecked(FALSE);

    settings.styleName = motif;
    settings.style = QApplication::setStyle(settings.styleName);
    kmtime->styleTimeControl(motif);
    settings.styleModified = TRUE;
}

void SettingsDialog::cdeStyleRadioButtonClicked()
{
    static char cde[] = "CDE";

    windowsStyleRadioButton->setChecked(FALSE);
    motifStyleRadioButton->setChecked(FALSE);
    cdeStyleRadioButton->setChecked(TRUE);
    motifPlusStyleRadioButton->setChecked(FALSE);
    platinumStyleRadioButton->setChecked(FALSE);
    sgiStyleRadioButton->setChecked(FALSE);
    macintoshRadioButton->setChecked(FALSE);
    aquaRadioButton->setChecked(FALSE);
    windowsXPRadioButton->setChecked(FALSE);

    settings.styleName = tr(cde);
    settings.style = QApplication::setStyle(settings.styleName);
    kmtime->styleTimeControl(cde);
    settings.styleModified = TRUE;
}

void SettingsDialog::motifPlusStyleRadioButtonClicked()
{
    static char motifplus[] = "MotifPlus";

    windowsStyleRadioButton->setChecked(FALSE);
    motifStyleRadioButton->setChecked(FALSE);
    cdeStyleRadioButton->setChecked(FALSE);
    motifPlusStyleRadioButton->setChecked(TRUE);
    platinumStyleRadioButton->setChecked(FALSE);
    sgiStyleRadioButton->setChecked(FALSE);
    macintoshRadioButton->setChecked(FALSE);
    aquaRadioButton->setChecked(FALSE);
    windowsXPRadioButton->setChecked(FALSE);

    settings.styleName = tr(motifplus);
    settings.style = QApplication::setStyle(settings.styleName);
    kmtime->styleTimeControl(motifplus);
    settings.styleModified = TRUE;
}

void SettingsDialog::platinumStyleRadioButtonClicked()
{
    static char platinum[] = "Platinum";

    windowsStyleRadioButton->setChecked(FALSE);
    motifStyleRadioButton->setChecked(FALSE);
    cdeStyleRadioButton->setChecked(FALSE);
    motifPlusStyleRadioButton->setChecked(FALSE);
    platinumStyleRadioButton->setChecked(TRUE);
    sgiStyleRadioButton->setChecked(FALSE);
    macintoshRadioButton->setChecked(FALSE);
    aquaRadioButton->setChecked(FALSE);
    windowsXPRadioButton->setChecked(FALSE);

    settings.styleName = tr(platinum);
    settings.style = QApplication::setStyle(settings.styleName);
    kmtime->styleTimeControl(platinum);
    settings.styleModified = TRUE;
}

void SettingsDialog::sgiStyleRadioButtonClicked()
{
    static char sgi[] = "SGI";

    windowsStyleRadioButton->setChecked(FALSE);
    motifStyleRadioButton->setChecked(FALSE);
    cdeStyleRadioButton->setChecked(FALSE);
    motifPlusStyleRadioButton->setChecked(FALSE);
    platinumStyleRadioButton->setChecked(FALSE);
    sgiStyleRadioButton->setChecked(TRUE);
    macintoshRadioButton->setChecked(FALSE);
    aquaRadioButton->setChecked(FALSE);
    windowsXPRadioButton->setChecked(FALSE);

    settings.styleName = tr(sgi);
    settings.style = QApplication::setStyle(settings.styleName);
    kmtime->styleTimeControl(sgi);
    settings.styleModified = TRUE;
}

void SettingsDialog::macintoshRadioButtonClicked()
{
    static char macintosh[] = "Macintosh";

    windowsStyleRadioButton->setChecked(FALSE);
    motifStyleRadioButton->setChecked(FALSE);
    cdeStyleRadioButton->setChecked(FALSE);
    motifPlusStyleRadioButton->setChecked(FALSE);
    platinumStyleRadioButton->setChecked(FALSE);
    sgiStyleRadioButton->setChecked(FALSE);
    macintoshRadioButton->setChecked(TRUE);
    aquaRadioButton->setChecked(FALSE);
    windowsXPRadioButton->setChecked(FALSE);

    settings.styleName = tr(macintosh);
    settings.style = QApplication::setStyle(settings.styleName);
    kmtime->styleTimeControl(macintosh);
    settings.styleModified = TRUE;
}

void SettingsDialog::aquaRadioButtonClicked()
{
    char aqua[] = "Aqua";

    windowsStyleRadioButton->setChecked(FALSE);
    motifStyleRadioButton->setChecked(FALSE);
    cdeStyleRadioButton->setChecked(FALSE);
    motifPlusStyleRadioButton->setChecked(FALSE);
    platinumStyleRadioButton->setChecked(FALSE);
    sgiStyleRadioButton->setChecked(FALSE);
    macintoshRadioButton->setChecked(FALSE);
    aquaRadioButton->setChecked(TRUE);
    windowsXPRadioButton->setChecked(FALSE);

    settings.styleName = tr(aqua);
    settings.style = QApplication::setStyle(settings.styleName);
    kmtime->styleTimeControl(aqua);
    settings.styleModified = TRUE;
}

void SettingsDialog::windowsXPRadioButtonClicked()
{
    static char windowsxp[] = "WindowsXP";

    windowsStyleRadioButton->setChecked(FALSE);
    motifStyleRadioButton->setChecked(FALSE);
    cdeStyleRadioButton->setChecked(FALSE);
    motifPlusStyleRadioButton->setChecked(FALSE);
    platinumStyleRadioButton->setChecked(FALSE);
    sgiStyleRadioButton->setChecked(FALSE);
    macintoshRadioButton->setChecked(FALSE);
    aquaRadioButton->setChecked(FALSE);
    windowsXPRadioButton->setChecked(TRUE);

    settings.styleName = tr(windowsxp);
    settings.style = QApplication::setStyle(settings.styleName);
    kmtime->styleTimeControl(windowsxp);
    settings.styleModified = TRUE;
}
