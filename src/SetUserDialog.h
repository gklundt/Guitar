#ifndef SETUSERDIALOG_H
#define SETUSERDIALOG_H

#include "Git.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class SetUserDialog;
}

class SetUserDialog : public QDialog
{
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	explicit SetUserDialog(MainWindow *parent, const Git::User &global_user, const Git::User &repo_user, const QString &repo);
	~SetUserDialog();

	bool isGlobalChecked() const;
	bool isRepositoryChecked() const;

	Git::User user() const;
private slots:
	void on_radioButton_global_toggled(bool checked);

	void on_radioButton_repository_toggled(bool checked);

	void on_lineEdit_name_textChanged(const QString &text);

	void on_lineEdit_mail_textChanged(const QString &text);

	void on_pushButton_get_icon_clicked();

private:
	Ui::SetUserDialog *ui;
	void setAvatar(QIcon icon);
	MainWindow *mainwindow();
};

#endif // SETUSERDIALOG_H
