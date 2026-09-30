#include "settingsdialog.h"

#include "app.h"
#include "config.h"
#include "platform.h"
#include "settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QLabel *note(const QString &text)
{
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setOpenExternalLinks(true);
    return label;
}

}  // namespace

SettingsDialog::SettingsDialog(App *app)
    : m_app(app)
{
    setWindowTitle(tr("%1 Settings").arg(QStringLiteral(APP_NAME)));
    setAttribute(Qt::WA_DeleteOnClose);

    auto *tabs = new QTabWidget;
    tabs->addTab(buildShortcutsTab(), tr("Shortcuts"));
    tabs->addTab(buildZoomTab(), tr("Zoom"));
    tabs->addTab(buildDrawTab(), tr("Draw"));
    tabs->addTab(buildSnipTab(), tr("Snip"));
    tabs->addTab(buildBreakTab(), tr("Break"));
    tabs->addTab(buildRecordTab(), tr("Record"));
    tabs->addTab(buildDemoTypeTab(), tr("DemoType"));
    tabs->addTab(buildGeneralTab(), tr("General"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
    resize(560, 520);
}

QWidget *SettingsDialog::buildShortcutsTab()
{
    auto *page = new QWidget;
    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("<b>Action</b>")), 0, 0);
    grid->addWidget(new QLabel(tr("<b>Shortcut</b>")), 0, 1);
    grid->addWidget(new QLabel(tr("<b>Alternative</b>")), 0, 2);
    int row = 1;
    for (Action action : configurableActions()) {
        grid->addWidget(new QLabel(actionInfo(action).label), row, 0);
        QList<QKeySequenceEdit *> editors;
        for (int column = 1; column <= 2; ++column) {
            auto *edit = new QKeySequenceEdit;
            connect(edit, &QKeySequenceEdit::editingFinished, this, [this, action] { saveShortcut(action); });
            grid->addWidget(edit, row, column);
            editors << edit;
        }
        m_editors.insert(int(action), editors);
        ++row;
    }

    auto *reset = new QPushButton(tr("Reset to defaults"));
    connect(reset, &QPushButton::clicked, this, [this] {
        m_app->settings()->resetShortcuts();
        loadShortcuts();
        m_app->applyShortcuts();
        updateStatus();
    });

    m_status = note(QString());
    auto *layout = new QVBoxLayout(page);
    layout->addLayout(grid);
    layout->addWidget(reset, 0, Qt::AlignLeft);
    layout->addWidget(m_status);
    layout->addWidget(note(tr("Command line: <tt>%1 zoom</tt>, <tt>draw</tt>, <tt>snip</tt>, <tt>snip-save</tt>, "
                              "<tt>break</tt>, <tt>livezoom</tt>. Bind these in your compositor if it has no "
                              "global shortcut service.")
                               .arg(QStringLiteral(APP_BIN))));
    layout->addStretch();
    loadShortcuts();
    updateStatus();
    return page;
}

void SettingsDialog::loadShortcuts()
{
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        const QList<QKeySequence> keys = m_app->settings()->shortcuts(Action(it.key()));
        for (int i = 0; i < it.value().size(); ++i) {
            QKeySequenceEdit *edit = it.value().at(i);
            QSignalBlocker blocker(edit);
            edit->setKeySequence(i < keys.size() ? keys.at(i) : QKeySequence());
        }
    }
}

void SettingsDialog::saveShortcut(Action action)
{
    QList<QKeySequence> keys;
    for (QKeySequenceEdit *edit : m_editors.value(int(action))) {
        QKeySequence sequence = edit->keySequence();
        if (sequence.count() > 1) {
            // Global shortcuts are single chords.
            sequence = QKeySequence(sequence[0]);
            QSignalBlocker blocker(edit);
            edit->setKeySequence(sequence);
        }
        if (!sequence.isEmpty())
            keys << sequence;
    }
    m_app->settings()->setShortcuts(action, keys);
    m_app->applyShortcuts();
    updateStatus();
}

void SettingsDialog::updateStatus()
{
    if (!m_status)
        return;
    QString text = tr("Shortcuts are registered through <b>%1</b> on %2 (%3).")
                       .arg(m_app->hotkeyBackend(), platform::desktopName(),
                            platform::isWayland() ? QStringLiteral("Wayland") : QStringLiteral("X11"));
    const QString problem = m_app->hotkeyStatus();
    if (!problem.isEmpty())
        text += QStringLiteral("<br><span style='color:#c01c28'>%1</span>").arg(problem.toHtmlEscaped());
    m_status->setText(text);
}

QWidget *SettingsDialog::buildZoomTab()
{
    Settings *s = m_app->settings();
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    auto *initial = new QDoubleSpinBox;
    initial->setRange(1.25, 8.0);
    initial->setSingleStep(0.25);
    initial->setSuffix(QStringLiteral("×"));
    initial->setValue(s->initialZoom());
    connect(initial, &QDoubleSpinBox::valueChanged, s, &Settings::setInitialZoom);
    form->addRow(tr("Initial magnification"), initial);

    auto *animate = new QCheckBox(tr("Animate zooming in and out"));
    animate->setChecked(s->animateZoom());
    connect(animate, &QCheckBox::toggled, s, &Settings::setAnimateZoom);
    form->addRow(QString(), animate);

    auto *smooth = new QCheckBox(tr("Smooth the zoomed image"));
    smooth->setChecked(s->smoothZoom());
    connect(smooth, &QCheckBox::toggled, s, &Settings::setSmoothZoom);
    form->addRow(QString(), smooth);

    auto *live = new QDoubleSpinBox;
    live->setRange(1.25, 8.0);
    live->setSingleStep(0.25);
    live->setSuffix(QStringLiteral("×"));
    live->setValue(s->liveZoomFactor());
    connect(live, &QDoubleSpinBox::valueChanged, s, &Settings::setLiveZoomFactor);
    form->addRow(tr("Live zoom magnification"), live);

    form->addRow(note(tr("While zoomed: move the mouse to pan, wheel or ↑/↓ to zoom in and out, left-click "
                         "to draw, Esc or right-click to leave. Ctrl+C copies and Ctrl+S saves what is shown; "
                         "add Shift to pick a region first.<br>Live zoom drives the desktop's own magnifier "
                         "(GNOME zoom, KWin Zoom effect); Ctrl+↑/↓ changes its magnification.")));
    return page;
}

QWidget *SettingsDialog::buildDrawTab()
{
    Settings *s = m_app->settings();
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    auto *color = new QPushButton;
    auto paintButton = [color](const QColor &c) {
        color->setText(c.name());
        color->setStyleSheet(QStringLiteral("background:%1; color:%2").arg(c.name(),
                                                                           c.lightness() > 140 ? "black" : "white"));
    };
    paintButton(s->penColor());
    connect(color, &QPushButton::clicked, this, [this, s, paintButton] {
        const QColor chosen = QColorDialog::getColor(s->penColor(), this);
        if (chosen.isValid()) {
            s->setPenColor(chosen);
            paintButton(chosen);
        }
    });
    form->addRow(tr("Starting pen colour"), color);

    auto *width = new QSpinBox;
    width->setRange(2, 40);
    width->setValue(s->penWidth());
    connect(width, &QSpinBox::valueChanged, s, &Settings::setPenWidth);
    form->addRow(tr("Pen width"), width);

    auto *font = new QFontComboBox;
    font->setCurrentFont(QFont(s->fontFamily()));
    connect(font, &QFontComboBox::currentFontChanged, s, [s](const QFont &f) { s->setFontFamily(f.family()); });
    form->addRow(tr("Text font"), font);

    auto *scale = new QSpinBox;
    scale->setRange(1, 50);
    scale->setPrefix(QStringLiteral("1/"));
    scale->setValue(s->fontScale());
    connect(scale, &QSpinBox::valueChanged, s, &Settings::setFontScale);
    form->addRow(tr("Text height (of the visible screen)"), scale);

    form->addRow(note(tr("<b>Drawing</b> (left-click while zoomed, or %1)<br>"
                         "R G B O Y P W K: red, green, blue, orange, yellow, pink, white, black<br>"
                         "Shift+colour: highlighter &nbsp;·&nbsp; X / Shift+X: blur / strong blur<br>"
                         "Drag: freehand &nbsp;·&nbsp; hold Shift: line &nbsp;·&nbsp; Ctrl: rectangle &nbsp;·&nbsp; "
                         "Tab: ellipse &nbsp;·&nbsp; Ctrl+Shift: arrow<br>"
                         "T / Shift+T: type (left / right aligned; wheel or ↑/↓ resizes)<br>"
                         "Ctrl+W / Ctrl+K: whiteboard / blackboard &nbsp;·&nbsp; E: erase all &nbsp;·&nbsp; "
                         "Ctrl+Z: undo<br>"
                         "Ctrl+wheel or Ctrl+↑/↓: pen width &nbsp;·&nbsp; wheel or ↑/↓: zoom<br>"
                         "Ctrl+C / Ctrl+S: copy / save the screen &nbsp;·&nbsp; with Shift: pick a region first<br>"
                         "Right-click: stop drawing &nbsp;·&nbsp; Esc: leave")
                          .arg(m_app->settings()->shortcuts(Action::Draw).value(0).toString(QKeySequence::NativeText))));
    return page;
}

QWidget *SettingsDialog::buildSnipTab()
{
    Settings *s = m_app->settings();
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    auto *dir = new QLineEdit(s->saveDirectory());
    connect(dir, &QLineEdit::editingFinished, s, [s, dir] { s->setSaveDirectory(dir->text()); });
    auto *browse = new QPushButton(tr("Browse…"));
    connect(browse, &QPushButton::clicked, this, [this, s, dir] {
        const QString chosen = QFileDialog::getExistingDirectory(this, tr("Save pictures to"), s->saveDirectory());
        if (!chosen.isEmpty()) {
            dir->setText(chosen);
            s->setSaveDirectory(chosen);
        }
    });
    auto *row = new QHBoxLayout;
    row->addWidget(dir);
    row->addWidget(browse);
    form->addRow(tr("Save folder"), row);

    auto *alsoSave = new QCheckBox(tr("Also save clipboard snips to this folder"));
    alsoSave->setChecked(s->snipAlsoSaves());
    connect(alsoSave, &QCheckBox::toggled, s, &Settings::setSnipAlsoSaves);
    form->addRow(QString(), alsoSave);

    form->addRow(note(tr("Snip freezes the screen: drag a rectangle to copy it, Enter takes the whole screen, "
                         "Esc cancels. Snip to file asks where to save. While zoomed, snipping copies what is "
                         "shown, drawings included.")));
    return page;
}

QWidget *SettingsDialog::buildBreakTab()
{
    Settings *s = m_app->settings();
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    auto *minutes = new QSpinBox;
    minutes->setRange(1, 99);
    minutes->setSuffix(tr(" min"));
    minutes->setValue(s->breakMinutes());
    connect(minutes, &QSpinBox::valueChanged, s, &Settings::setBreakMinutes);
    form->addRow(tr("Timer length"), minutes);

    auto *elapsed = new QCheckBox(tr("Show time elapsed after expiration"));
    elapsed->setChecked(s->breakShowElapsed());
    connect(elapsed, &QCheckBox::toggled, s, &Settings::setBreakShowElapsed);
    form->addRow(QString(), elapsed);

    auto *sound = new QCheckBox(tr("Play a sound on expiration"));
    sound->setChecked(s->breakPlaySound());
    connect(sound, &QCheckBox::toggled, s, &Settings::setBreakPlaySound);
    auto *soundFile = new QLineEdit(s->breakSoundFile());
    soundFile->setPlaceholderText(tr("Desktop alarm sound"));
    connect(soundFile, &QLineEdit::editingFinished, s, [s, soundFile] { s->setBreakSoundFile(soundFile->text()); });
    auto *browseSound = new QPushButton(tr("Browse…"));
    connect(browseSound, &QPushButton::clicked, this, [this, s, soundFile] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Alarm sound"), QString(),
                                                          tr("Sounds (*.oga *.ogg *.wav *.flac *.mp3)"));
        if (!file.isEmpty()) {
            soundFile->setText(file);
            s->setBreakSoundFile(file);
        }
    });
    auto *soundRow = new QHBoxLayout;
    soundRow->addWidget(soundFile);
    soundRow->addWidget(browseSound);
    form->addRow(QString(), sound);
    form->addRow(tr("Alarm sound"), soundRow);

    auto *opacity = new QSpinBox;
    opacity->setRange(10, 100);
    opacity->setSingleStep(10);
    opacity->setSuffix(QStringLiteral("%"));
    opacity->setValue(s->breakOpacity());
    connect(opacity, &QSpinBox::valueChanged, s, &Settings::setBreakOpacity);
    form->addRow(tr("Timer opacity"), opacity);

    auto *position = new QComboBox;
    const QStringList positions = {tr("Top left"),    tr("Top"),    tr("Top right"),
                                   tr("Left"),        tr("Centre"), tr("Right"),
                                   tr("Bottom left"), tr("Bottom"), tr("Bottom right")};
    position->addItems(positions);
    position->setCurrentIndex(s->breakPosition());
    connect(position, &QComboBox::currentIndexChanged, s, &Settings::setBreakPosition);
    form->addRow(tr("Timer position"), position);

    auto *background = new QComboBox;
    background->addItems({tr("Plain colour (Ctrl+W white, Ctrl+K black)"), tr("Faded desktop"), tr("Image file")});
    background->setCurrentIndex(int(s->breakBackground()));
    connect(background, &QComboBox::currentIndexChanged, s,
            [s](int index) { s->setBreakBackground(Settings::BreakBackground(index)); });
    form->addRow(tr("Background"), background);

    auto *imageFile = new QLineEdit(s->breakImageFile());
    connect(imageFile, &QLineEdit::editingFinished, s, [s, imageFile] { s->setBreakImageFile(imageFile->text()); });
    auto *browseImage = new QPushButton(tr("Browse…"));
    connect(browseImage, &QPushButton::clicked, this, [this, s, imageFile] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Background image"), QString(),
                                                          tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (!file.isEmpty()) {
            imageFile->setText(file);
            s->setBreakImageFile(file);
        }
    });
    auto *imageRow = new QHBoxLayout;
    imageRow->addWidget(imageFile);
    imageRow->addWidget(browseImage);
    form->addRow(tr("Background image"), imageRow);

    auto *scaleImage = new QCheckBox(tr("Scale the image to the screen"));
    scaleImage->setChecked(s->breakScaleImage());
    connect(scaleImage, &QCheckBox::toggled, s, &Settings::setBreakScaleImage);
    form->addRow(QString(), scaleImage);

    form->addRow(note(tr("↑/↓ or wheel: ±1 minute &nbsp;·&nbsp; ←/→: ±10 seconds &nbsp;·&nbsp; "
                         "R G B O Y P W K: text colour &nbsp;·&nbsp; Esc or right-click: close. "
                         "Switch away with Alt+Tab; the break hotkey brings the timer back.")));
    return page;
}

QWidget *SettingsDialog::buildRecordTab()
{
    Settings *s = m_app->settings();
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    auto *dir = new QLineEdit(s->recordDirectory());
    connect(dir, &QLineEdit::editingFinished, s, [s, dir] { s->setRecordDirectory(dir->text()); });
    auto *browse = new QPushButton(tr("Browse…"));
    connect(browse, &QPushButton::clicked, this, [this, s, dir] {
        const QString chosen = QFileDialog::getExistingDirectory(this, tr("Save recordings to"), s->recordDirectory());
        if (!chosen.isEmpty()) {
            dir->setText(chosen);
            s->setRecordDirectory(chosen);
        }
    });
    auto *row = new QHBoxLayout;
    row->addWidget(dir);
    row->addWidget(browse);
    form->addRow(tr("Save folder"), row);

    auto *audio = new QCheckBox(tr("Record system sound"));
    audio->setChecked(s->recordAudio());
    connect(audio, &QCheckBox::toggled, s, &Settings::setRecordAudio);
    form->addRow(QString(), audio);

    auto *fps = new QSpinBox;
    fps->setRange(5, 60);
    fps->setSuffix(tr(" fps"));
    fps->setValue(s->recordFrameRate());
    connect(fps, &QSpinBox::valueChanged, s, &Settings::setRecordFrameRate);
    form->addRow(tr("Frame rate"), fps);

    form->addRow(note(tr("The record hotkey starts and stops a recording of the monitor (under Wayland the "
                         "desktop asks which one, once). With Shift you pick a region first; with Alt it "
                         "records one window. Zooming and drawing show up in the recording. Files are MP4 "
                         "(H.264), or WebM when no H.264 encoder is installed.")));
    return page;
}

QWidget *SettingsDialog::buildDemoTypeTab()
{
    Settings *s = m_app->settings();
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    auto *file = new QLineEdit(s->demoTypeFile());
    connect(file, &QLineEdit::editingFinished, s, [s, file] { s->setDemoTypeFile(file->text()); });
    auto *browse = new QPushButton(tr("Browse…"));
    connect(browse, &QPushButton::clicked, this, [this, s, file] {
        const QString chosen = QFileDialog::getOpenFileName(this, tr("DemoType script"), s->demoTypeFile(),
                                                            tr("Text files (*.txt *.md);;All files (*)"));
        if (!chosen.isEmpty()) {
            file->setText(chosen);
            s->setDemoTypeFile(chosen);
        }
    });
    auto *row = new QHBoxLayout;
    row->addWidget(file);
    row->addWidget(browse);
    form->addRow(tr("Script file"), row);

    auto *speed = new QSlider(Qt::Horizontal);
    speed->setRange(10, 100);
    speed->setValue(s->demoTypeSpeed());
    connect(speed, &QSlider::valueChanged, s, &Settings::setDemoTypeSpeed);
    auto *speedRow = new QHBoxLayout;
    speedRow->addWidget(new QLabel(tr("Slow")));
    speedRow->addWidget(speed);
    speedRow->addWidget(new QLabel(tr("Fast")));
    form->addRow(tr("Typing speed"), speedRow);

    form->addRow(note(tr("Each press of the DemoType hotkey types the next snippet into the focused window. "
                         "Separate snippets with <tt>[end]</tt>. Also: <tt>[pause:n]</tt> waits n seconds, "
                         "<tt>[enter]</tt> <tt>[up]</tt> <tt>[down]</tt> <tt>[left]</tt> <tt>[right]</tt> press "
                         "keys, and <tt>[paste]</tt>…<tt>[/paste]</tt> pastes a block at once. Text on the "
                         "clipboard that starts with <tt>[start]</tt> is used instead of the file. With Shift, "
                         "the hotkey steps back one snippet; Esc stops typing.<br>Under Wayland the desktop asks "
                         "once whether %1 may type for you.")
                          .arg(QStringLiteral(APP_NAME))));
    return page;
}

QWidget *SettingsDialog::buildGeneralTab()
{
    Settings *s = m_app->settings();
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    auto *login = new QCheckBox(tr("Start %1 when I log in").arg(QStringLiteral(APP_NAME)));
    login->setChecked(s->startAtLogin());
    connect(login, &QCheckBox::toggled, this, [this, s](bool on) {
        s->setStartAtLogin(on);
        m_app->applyGeneralSettings();
    });
    form->addRow(QString(), login);

    auto *tray = new QCheckBox(tr("Show an icon in the system tray"));
    tray->setChecked(s->showTrayIcon());
    connect(tray, &QCheckBox::toggled, this, [this, s](bool on) {
        s->setShowTrayIcon(on);
        m_app->applyGeneralSettings();
    });
    form->addRow(QString(), tray);

    form->addRow(note(tr("<b>%1 %2</b><br>Session: %3, %4<br>Screen capture: %5<br>Shortcuts: %6<br>"
                         "Settings file: %7")
                          .arg(QStringLiteral(APP_NAME), QStringLiteral(APP_VERSION), platform::desktopName(),
                               platform::isWayland() ? QStringLiteral("Wayland") : QStringLiteral("X11"),
                               m_app->captureBackend(), m_app->hotkeyBackend(),
                               s->fileName().toHtmlEscaped())));
    return page;
}
