#include "libdialog.h"
#include <qfiledialog.h>

libraryDialog::libraryDialog(ssmp *parent, Qt::WFlags flags)
	: QDialog(parent, flags)
{
	myparent = parent;
	ui.setupUi(this);
	connect(ui.browseBtn,SIGNAL(clicked()),this,SLOT(getDir()));
	if(parent->settings->value("libdir") != QVariant())
		ui.libDirTxt->setText(parent->settings->value("libdir").toString());
}

void libraryDialog::getDir()
{
	libdir = QFileDialog::getExistingDirectory(this, "Select Libdir",ui.libDirTxt->text(),
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	ui.libDirTxt->setText(libdir);
}

void libraryDialog::accept()
{
	myparent->settings->setValue("libdir", ui.libDirTxt->text());	
	ui.buttonBox->setDisabled(true);
	addDir2Lib(ui.libDirTxt->text());
	this->close();
}

//Adds dir & its contents to the library
void libraryDialog::addDir2Lib(QDir dir)
{
	dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);
	QDirIterator di(dir, QDirIterator::Subdirectories);	

	while(di.hasNext())
	{
		di.next();		
		QString fpath = di.filePath();
		QFileInfo f = di.fileInfo();
		if(isAudioFile(f))//Add this song to the database
		{
			wchar_t wname[250]; //TODO: Dynamic. Need to figure out wchar length from QStr length
			wname[fpath.toWCharArray(wname)] = 0;
			TagLib::FileName fname(wname);
			//We'll store tag information in these:
			QMap<QString, QString> stmap;
			QMap<QString, int> itmap;
			
			TagLib::File* file = NULL;
			//MP3 Means we can check for additional info in ID3v2 tags
			if(f.suffix() == "mp3")
			{				
				TagLib::MPEG::File* fr = new TagLib::MPEG::File(fname, true, TagLib::AudioProperties::ReadStyle::Fast);				
				if(fr->ID3v2Tag())
				{					
					//Somehow this means album artist / band. http://www.id3.org/id3v2.4.0-frames
					TagLib::ID3v2::FrameList l = fr->ID3v2Tag()->frameList("TPE2");
					if(!l.isEmpty())
						stmap["albumartist"] = l.front()->toString().toCString();
				}
				file = dynamic_cast<TagLib::File*>(fr);
			}
			if(file == NULL)
			{
				qDebug() << "ERR: " + fpath;
				continue; //TODO: Error out here
			}
			//Try to get audio properties
			TagLib::AudioProperties* ap = file->audioProperties();			
			
			TagLib::Tag* genTag = file->tag();			
			stmap["name"] = genTag->title().toCString();
			stmap["genre"] = genTag->genre().toCString();
			itmap["year"] = genTag->year();
			itmap["tracknum"] = genTag->track();
			stmap["album"] = genTag->album().toCString();						
			stmap["artist"] = genTag->artist().toCString();			
			if(ap != NULL)
				itmap["length"] = ap->length();
			stmap["path"] = fpath;	
			//Add collected info to db
			DBItem s;
			s.strVals = stmap;
			s.intVals = itmap;

			myparent->dbi->addSong(s);
			delete file;
		}
		else if(f.isDir())
			ui.curDirLbl->setText(fpath);
		//if(top) //If we're the top level of recursion update prog bar
		//	ui.progressBar->setValue(di./siz * 100);		
		qApp->processEvents();
	}
}

bool libraryDialog::isAudioFile(QFileInfo f)
{
	QString type = f.suffix();
	return std::accumulate(myparent->supportedFileFormats.begin(), myparent->supportedFileFormats.end(), false,
		[type](bool a, std::string b) {return (a || b == type.toStdString());});
}

libraryDialog::~libraryDialog()
{
}
