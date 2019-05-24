/*
 * GameLibraryUI.hpp
 *
 *  Created on: Dec 5, 2016
 *      Author: swatson
 */

#pragma once

#include "GenesisViewUI.hpp"


#include <cerrno>


#include <fstream>
#include <numeric>
#include <iterator>
#include <algorithm>


#include <QObject>


// Data Sources
#include <bb/data/JsonDataAccess>

using namespace bb::data;


// File Picker
#include <bb/cascades/pickers/FilePicker>

using namespace bb::cascades::pickers;


// Resources
#include <bb/cascades/Color>
#include <bb/cascades/Image>
#include <bb/cascades/QmlDocument>

// Core Controls
#include <bb/cascades/Option>
#include <bb/cascades/Container>
#include <bb/cascades/ScrollView>
#include <bb/cascades/Theme>
#include <bb/cascades/UIPalette>
#include <bb/cascades/ColorTheme>
#include <bb/cascades/ThemeSupport>
#include <bb/cascades/ProgressIndicator>
#include <bb/cascades/ActivityIndicator>

// List
#include <bb/cascades/Header>
#include <bb/cascades/ListView>
#include <bb/cascades/CustomListItem>
#include <bb/cascades/ListItemTypeMapper>

// Application Structure
#include <bb/cascades/Application>
#include <bb/cascades/Page>
#include <bb/cascades/ImageView>
#include <bb/cascades/ActionSet>
#include <bb/cascades/ActionItem>
#include <bb/cascades/HelpActionItem>
#include <bb/cascades/DeleteActionItem>
#include <bb/cascades/SettingsActionItem>
#include <bb/cascades/MultiSelectActionItem>

// System UI
#include <bb/cascades/Menu>
#include <bb/cascades/TitleBar>
#include <bb/cascades/TitleBarKind>
#include <bb/system/SystemDialog>
#include <bb/system/SystemPrompt>
#include <bb/system/SystemToast>
#include <bb/system/SystemUiResult>
#include <bb/system/SystemProgressToast>
#include <bb/system/SystemUiProgressState>
#include <bb/system/SystemUiPosition>

// Text
#include <bb/cascades/Label>
#include <bb/cascades/LabelAutoSizeProperties>
#include <bb/cascades/LabelTextFitProperties>
#include <bb/cascades/TextField>
#include <bb/cascades/TextStyle>
#include <bb/cascades/TextStyleDefinition>

// Layouts
#include <bb/cascades/DockLayout>
#include <bb/cascades/StackLayout>
#include <bb/cascades/GridListLayout>
#include <bb/cascades/StackListLayout>

// Data Models
#include <bb/cascades/ArrayDataModel>

// Animations
#include <bb/cascades/FadeTransition>
#include <bb/cascades/StockCurve>

using namespace bb::system;
using namespace bb::cascades;

// TODO Add cancel button during box art download.
// TODO Add save state support.
// TODO Add settings screen. Image at top.
/*
 * The internal database used for the UI is in the form
 * of an array of objects. All games are located in it's
 * respective directory and named game.bin.
 *
 * [
 *     {
 *         "gameID": "0",
 *         "title": "game's title",
 *         "settings": {
 *              "vsync": true,
 *              "fullscreen": true,
 *              "title": true,
 *              ...
 *         },
 *         "states": [
 *              "gameId_0.gp0",
 *              "gameId_0.gp0",
 *              ...
 *         ]
 *     },
 *     ...
 * ]
 *
 * gameId: a string that contain the game's unique id.
 * title: a string that contain the game's title.
 * settings: a map that contain the game's custom settings.
 * states: an array of strings that contain game's path to the saved state file from the cwd.
 *
 * settings:
 *     spec TBD
 *
 * states:
 *     Saved states are put in the data folder and, given
 *     the extension gp0. All saved states have a corresponding
 *     image associated with them. The image has the same file
 *     name as the saved state file but, with the extension bmp.
 * */
class GameLibraryUI: public QObject
{
    Q_OBJECT

    GenesisViewUI genesis_view_ui;

    ArrayDataModel *data_model = new ArrayDataModel( this );

    Page *game_library_view = new Page( this );
    Page *saved_state_view = new Page( this );

    Container *game_library_content = Container::create().parent( game_library_view );
    Container *saved_state_content = Container::create().parent( saved_state_view );

    /*
     *
     * */
    class OptionForm: public Sheet
    {
    public:
        Page *page;

        explicit OptionForm(QObject *parent = 0): Sheet((UIObject*)parent)
        {
            page = Page::create().parent(this)
                                 .titleBar( TitleBar::create().parent( this )
                                                              .acceptAction( ActionItem::create().parent(this).title("Save") )
                                                              .dismissAction( ActionItem::create().parent(this).title("Cancel").onTriggered( this, SLOT(close()) ) ) );
            setContent( page );
        }

        QVariantMap result()
        {
            return QVariantMap();
        }
    };
    OptionForm *option_sheet = new OptionForm(this);
    FilePicker *boxart_picker = new FilePicker(this);
    SystemPrompt *rename_prompt = new SystemPrompt(this);
    SystemDialog *delete_dialog = new SystemDialog("Delete", "Cancel", this);
    FilePicker *import_picker = new FilePicker(this);
    QPointer<QFutureWatcher<void>> import_watcher;
    QMutex import_watcher_mutex;

    /*
     *      Create a segmented title bar for UI.
     * */
    Option *library_option = Option::create().parent( this )
                                             .selected( true )
                                             .text( "Library" )
                                             .onSelectedChanged( this, SLOT( onLibraryOption(bool) ) );
    Option *saves_option = Option::create().parent( this )
                                           .selected( false )
                                           .text( "Saved States" )
                                           .onSelectedChanged( this, SLOT( onSavesOption(bool) ) );

    TitleBar *segmented_bar = TitleBar::create( TitleBarKind::Segmented ).parent( this )
                                                                         .addOption( library_option )
                                                                         .addOption( saves_option );

    /*
     *      Create a main menu.
     * */
    Menu *main_menu = Menu::create().parent( this )
                                    .help( HelpActionItem::create().parent( this ) )
                                    .settings( SettingsActionItem::create().parent( this ) )
                                    .addAction( ActionItem::create().parent( this ).title( "About" ).imageSource( QUrl("asset:///ic_info.png") ) );


    Q_SLOT void loadDataBase()
    {
        JsonDataAccess jda;

        const auto &data = jda.load("data/library.json");

        if( !jda.hasError() )
            data_model->append( data.toList() );
    }


    Q_SLOT void saveDataBase()
    {
        JsonDataAccess jda;

        QVariantList data;
        for(int i = 0; i < data_model->size(); i++)
            if( data_model->value(i).toMap().isEmpty() )
                continue;
            else
                data << data_model->value(i);

        jda.save(data, "data/library.json");
    }


    Q_SLOT void onLibraryOption( bool selected )
    {
        if( selected )
        {
            ((FadeTransition*)FadeTransition::create(saved_state_content).connect( SIGNAL(ended()), this, SLOT(fadeInLibrary()) )
                                                                         .autoDeleted(true)
                                                                         .easingCurve(StockCurve::Linear)
                                                                         .from(1.0f)
                                                                         .to(0.0f)
                                                                         .duration(120)
                                                                         .parent( saved_state_content ))->play();
        }
    }


    Q_SLOT void fadeInLibrary()
    {
        game_library_view->setTitleBar( segmented_bar );
        Application::instance()->setScene( game_library_view );
        ((FadeTransition*)FadeTransition::create(game_library_content).autoDeleted(true)
                                                                      .easingCurve(StockCurve::Linear)
                                                                      .from(0.0f)
                                                                      .to(1.0f)
                                                                      .duration(120)
                                                                      .parent( game_library_content ))->play();
    }


    Q_SLOT void onSavesOption( bool selected )
    {
        if( selected )
        {
            ((FadeTransition*)FadeTransition::create(game_library_content).connect( SIGNAL(ended()), this, SLOT(fadeInSaves()) )
                                                                          .autoDeleted(true)
                                                                          .easingCurve(StockCurve::Linear)
                                                                          .from(1.0f)
                                                                          .to(0.0f)
                                                                          .duration(120)
                                                                          .parent( game_library_content ))->play();
        }
    }


    Q_SLOT void fadeInSaves()
    {
        saved_state_view->setTitleBar( segmented_bar );
        Application::instance()->setScene( saved_state_view );
        ((FadeTransition*)FadeTransition::create(saved_state_content).autoDeleted(true)
                                                                     .easingCurve(StockCurve::Linear)
                                                                     .from(0.0f)
                                                                     .to(1.0f)
                                                                     .duration(120)
                                                                     .parent( saved_state_content ))->play();
    }


    Q_SLOT void onLibraryListTriggered(QVariantList indexPath)
    {
        if(!genesis_view_ui.isRunning() && !data_model->data(indexPath).toMap().isEmpty())
            genesis_view_ui.openROM( data_model->data(indexPath).toMap() );
    }


    Q_SLOT void onSavesListTriggered(QVariantList indexPath)
    {
        qDebug() << data_model->data(indexPath);
    }


    Q_SLOT void showRenameGamePrompt(int index)
    {
        QSignalMapper *rename_signal_remap = new QSignalMapper(this);
        rename_signal_remap->setMapping(rename_prompt, index);

        bool connection;
        Q_UNUSED( connection );
        connection = connect(rename_signal_remap, SIGNAL(mapped(int)), this, SLOT(renameGame(int)));
        Q_ASSERT( connection );
        connection = connect(rename_prompt, SIGNAL(finished(bb::system::SystemUiResult::Type)), rename_signal_remap, SLOT(map()));
        Q_ASSERT( connection );
        connection = connect(rename_prompt, SIGNAL(finished(bb::system::SystemUiResult::Type)), rename_signal_remap, SLOT(deleteLater()));
        Q_ASSERT( connection );

        rename_prompt->setTitle("Rename " + data_model->value(index).toMap().value("title").toString() + ".");
        rename_prompt->show();
    }


    Q_SLOT void renameGame(int index)
    {
        if( rename_prompt->result() == bb::system::SystemUiResult::ConfirmButtonSelection)
        {
            QVariantMap entry = data_model->value(index).toMap();
            entry["title"] = rename_prompt->inputFieldTextEntry();

            data_model->replace(index, entry);
            saveDataBase();
        }
    }


    Q_SLOT void showDeleteGamePrompt(int index)
    {
        QSignalMapper *delete_signal_remap = new QSignalMapper(this);
        delete_signal_remap->setMapping(delete_dialog, index);

        bool connection;
        Q_UNUSED( connection );
        connection = connect(delete_signal_remap, SIGNAL(mapped(int)), this, SLOT(deleteGame(int)));
        Q_ASSERT( connection );
        connection = connect(delete_dialog, SIGNAL(finished(bb::system::SystemUiResult::Type)), delete_signal_remap, SLOT(map()));
        Q_ASSERT( connection );
        connection = connect(delete_dialog, SIGNAL(finished(bb::system::SystemUiResult::Type)), delete_signal_remap, SLOT(deleteLater()));
        Q_ASSERT( connection );

        delete_dialog->setTitle("Confirm Delete");
        delete_dialog->setBody("Delete " + data_model->value(index).toMap().value("title").toString() + "?");
        delete_dialog->show();
    }


    Q_SLOT void deleteGame(int index)
    {
        if( delete_dialog->result() == bb::system::SystemUiResult::ConfirmButtonSelection)
        {
            qDebug() << QFile::remove( "data/" + data_model->value(index).toMap().value( "gameID" ).toString() + ".bin" );
            qDebug() << QFile::remove( "data/" + data_model->value(index).toMap().value( "gameID" ).toString() + ".img" );

            for( const auto &state : data_model->value(index).toMap().value( "states" ).toList() )
            {
                qDebug() << QFile::remove( "data/" + state.toString() );
            }

            data_model->removeAt(index);
            saveDataBase();
        }
    }


    Q_SLOT void showSetGameArtPrompt(int index)
    {
        QSignalMapper *boxart_signal_remap = new QSignalMapper(this);
        boxart_signal_remap->setMapping(boxart_picker, index);

        bool connection;
        Q_UNUSED( connection );
        connection = connect( boxart_signal_remap, SIGNAL(mapped(int)), this, SLOT(setGameArt(int)) );
        Q_ASSERT( connection );
        connection = connect( boxart_picker, SIGNAL(fileSelected(const QStringList&)), boxart_signal_remap, SLOT(map()) );
        Q_ASSERT( connection );
        connection = connect( boxart_picker, SIGNAL(fileSelected(const QStringList&)), boxart_signal_remap, SLOT(deleteLater()) );
        Q_ASSERT( connection );
        connection = connect( boxart_picker, SIGNAL(canceled()), boxart_signal_remap, SLOT(deleteLater()) );
        Q_ASSERT( connection );

        boxart_picker->open();
    }


    Q_SLOT void setGameArt(int index)
    {
        // Copy file
        const char *src_string = boxart_picker->selectedFiles()[0].toAscii().constData();
        const char *dst_string = ("data/" + data_model->value(index).toMap().value("gameID").toString() + ".img").toAscii().constData();
        std::ifstream src_file( src_string, std::ifstream::binary | std::ifstream::in );
        std::ofstream dst_file( dst_string, std::ifstream::binary | std::ifstream::out );
        std::copy( std::istreambuf_iterator<char>(src_file),
                   std::istreambuf_iterator<char>(),
                   std::ostreambuf_iterator<char>(dst_file) );
        dst_file.flush();

        data_model->replace(index, data_model->value(index).toMap());
    }


    Q_SLOT void showGameOptionPrompt(int index)
    {
        QSignalMapper *option_signal_remap = new QSignalMapper(this);
        option_signal_remap->setMapping(option_sheet->page->titleBar()->acceptAction(), index);

        bool connection;
        Q_UNUSED( connection );
        connection = connect( option_signal_remap, SIGNAL(mapped(int)), this, SLOT(gameOption(int)) );
        Q_ASSERT( connection );
        connection = connect( option_sheet->page->titleBar()->acceptAction(), SIGNAL(triggered()), option_signal_remap, SLOT(map()) );
        Q_ASSERT( connection );
        connection = connect( option_sheet, SIGNAL(closed()), option_signal_remap, SLOT(deleteLater()) );
        Q_ASSERT( connection );

        option_sheet->page->titleBar()->setTitle( data_model->value(index).toMap().value("title").toString() );
        option_sheet->open();
    }


    Q_SLOT void gameOption(int index)
    {
//        QVariantMap entry = data_model->value(index).toMap();
//        QVariantList settings = data_model->value(index).toMap().contains("settings") ? data_model->value(index).toMap().value("settings").toList() : QVariantList();
//        ...
//        entry["settings"] = settings;
//
//        data_model->replace(index, option_sheet->result());
//        saveDataBase();
        qDebug() << data_model->value(index).toMap().value("title").toString();
        option_sheet->close();
    }


    Q_SLOT void importGames(const QStringList& selectedFiles)
    {
        struct ProccessFile
        {
            QAtomicPointer<GameLibraryUI> instance;

            ProccessFile(GameLibraryUI *caller): instance(caller) {}


            void operator()(const QString &filePath)
             {
                std::ifstream rom_file( filePath.toAscii().constData(), std::ifstream::binary | std::ifstream::in );

                if( rom_file.is_open() )
                {
                    int at = instance->data_model->size();
                    instance->data_model->append( QVariantMap() );

                    // Calculate CRC
                    const auto &crc_string = QString::number( std::accumulate( std::istreambuf_iterator<char>(rom_file),
                                                                               std::istreambuf_iterator<char>(),
                                                                               crc32(0L, Z_NULL, 0),
                                                                               [](uLong crc, Bytef byte) {
                                                                                   return crc32( crc, &byte, sizeof(char) );
                                                                               } ), 16 ).toUpper();


                    // Look for entry in database.
                    const auto rbegin = std::reverse_iterator<QList<QVariant>::const_iterator>( instance->import_watcher->property("db").toList().constEnd() );
                    const auto rend   = std::reverse_iterator<QList<QVariant>::const_iterator>( instance->import_watcher->property("db").toList().constBegin() );

                    auto found = std::find_if( rbegin, rend,
                                               [crc_string](const QVariant &entry) {
                                                   return entry.toMap().value("romHashCRC") == crc_string;
                                               } );

                    const auto &found_entry = (found != rend) ? found->toMap() : QVariantMap();


                    if( instance->import_watcher->isCanceled() )
                        return instance->data_model->removeAt(at);

                    // Download cover art if any.
                    if( found_entry.contains("releaseCoverFront") && !QFileInfo("data/" + crc_string + ".img").exists() )
                    {
                        QScopedPointer<QNetworkAccessManager> manager( new QNetworkAccessManager );
                        QScopedPointer<QNetworkReply> reply( manager->get( QNetworkRequest( found_entry.value("releaseCoverFront").toUrl() ) ) );

                        QEventLoop loop;
                        loop.connect( instance->import_watcher, SIGNAL(canceled()), SLOT(quit()) );
                        loop.connect( reply.data(), SIGNAL(finished()), SLOT(quit()) );
                        loop.exec();

                        if( !reply->isFinished() )
                            return instance->data_model->removeAt(at);

                        if( reply->error() == QNetworkReply::NoError )
                        {
                            QImage img;
                            img.loadFromData( reply->readAll() );
                            img.scaledToWidth(100, Qt::SmoothTransformation).save( "data/" + crc_string + ".img", "png" );
                        }
                    }


                    // Copy or Move File
                    rom_file.seekg(0, std::ios_base::beg);
                    std::ofstream dst_file( ("data/" + crc_string + ".bin").toAscii().constData(), std::ifstream::binary | std::ifstream::out );
                    std::copy( std::istreambuf_iterator<char>(rom_file),
                               std::istreambuf_iterator<char>(),
                               std::ostreambuf_iterator<char>(dst_file) );
                    dst_file.flush();


                    // Add to database.
                    QVariantMap settings;
                    settings["title"] = !QFileInfo("data/" + crc_string + ".img").exists();

                    QVariantMap entry;
                    entry["gameID"] = crc_string;
                    entry["title"]  = found_entry.contains("releaseTitleName") ? found_entry.value("releaseTitleName").toString() : "";
                    entry["settings"] = settings;

                    instance->data_model->replace(at, entry);


                    QMutexLocker lk(&instance->import_watcher_mutex);

                    JsonDataAccess jda;
                    auto data = jda.load("data/library.json").toList();
                    data.append(entry);
                    jda.save(data, "data/library.json");
                }
                else
                {
                    if(errno == EACCES)
                    {
                        qErrnoWarning("proccessFile: ");
                        qDebug() << "TODO - ask for permission.";
                    }
                }
             }
        };


        JsonDataAccess jda;

        import_watcher = new QFutureWatcher<void>(this);
        import_watcher->connect( import_watcher.data(), SIGNAL( finished() ), SLOT( deleteLater() ) );
        import_watcher->setProperty( "db", jda.load("app/native/assets/games.json") );
        import_watcher->setFuture( QtConcurrent::map( selectedFiles, ProccessFile(this) ) );
    }


    Q_INVOKABLE void buildUI()
    {
        /*
         *      Start building the library screen.
         * */
        class GameItemTypeMapper : public ListItemTypeMapper, public QObject
        {
            QString itemType( const QVariant& data, const QVariantList& indexPath __attribute__((unused)) ) override
            {
                return data.toMap().isEmpty() ? "" : "game";
            }

        public:
            GameItemTypeMapper(QObject* parent) : QObject(parent)
            {
            }
        };

        class GameItemProvider: public ListItemProvider
        {
            const Image nobox_icon;

            QSignalMapper *rename_signal_map = new QSignalMapper(this);
            QSignalMapper *boxart_signal_map = new QSignalMapper(this);
            QSignalMapper *option_signal_map = new QSignalMapper(this);
            QSignalMapper *delete_signal_map = new QSignalMapper(this);

            VisualNode *createItem( ListView *list, const QString &type ) override
            {
                if(type == "game")
                {
                    Label *title_label = Label::create().parent( list )
                                                        .multiline( true )
                                                        .horizontal( HorizontalAlignment::Fill );
                    title_label->textStyle()->setFontSize( FontSize::XSmall );
                    title_label->textStyle()->setTextAlign( TextAlign::Center );

                    return CustomListItem::create().parent( list )
                                                   .content( Container::create().parent( list )
                                                                                .vertical( VerticalAlignment::Fill )
                                                                                .horizontal( HorizontalAlignment::Fill )
                                                                                .layout( DockLayout::create().parent( list ) )
                                                                                .add( ImageView::create().parent( list )
                                                                                                         .vertical( VerticalAlignment::Fill )
                                                                                                         .horizontal( HorizontalAlignment::Fill )
                                                                                                         .scalingMethod( ScalingMethod::AspectFit )
                                                                                                         .loadEffect( ImageViewLoadEffect::Subtle ) )
                                                                                 .add( Container::create().parent( list )
                                                                                                          .top( list->ui()->du(0.5f) )
                                                                                                          .bottom( list->ui()->du(0.5f) )
                                                                                                          .background( list->ui()->palette()->background() )
                                                                                                          .opacity( 0.75f )
                                                                                                          .vertical( VerticalAlignment::Bottom )
                                                                                                          .horizontal( HorizontalAlignment::Fill )
                                                                                                          .add( title_label ) ) )
                                                   .actionSet( ActionSet::create().parent( list )
                                                                                  .add( ActionItem::create().parent( list ).title( "Rename" ).imageSource( QUrl("asset:///ic_rename.png") ).onTriggered( rename_signal_map, SLOT(map()) ) )
                                                                                  .add( ActionItem::create().parent( list ).title( "Set Image" ).imageSource( QUrl("asset:///ic_view_image.png") ).onTriggered( boxart_signal_map, SLOT(map()) ) )
                                                                                  .add( ActionItem::create().parent( list ).title( "Settings" ).onTriggered( option_signal_map, SLOT(map()) ) )
                                                                                  .add( DeleteActionItem::create().parent( list ).onTriggered( delete_signal_map, SLOT(map()) ) ) );
                }
                else
                {
                    return Container::create().parent( list )
                                              .vertical( VerticalAlignment::Fill )
                                              .horizontal( HorizontalAlignment::Fill )
                                              .layout( DockLayout::create().parent( list ) )
                                              .add( ActivityIndicator::create().parent( list )
                                                                               .vertical( VerticalAlignment::Fill )
                                                                               .horizontal( HorizontalAlignment::Fill ) );
                }
            }

            void updateItem( ListView           *list      __attribute__((unused)),
                             VisualNode         *listItem,
                             const QString      &type,
                             const QVariantList &indexPath,
                             const QVariant     &data ) override
            {
                if(type == "game")
                {
                    auto *list_item = qobject_cast<CustomListItem*>( listItem );
                    auto *content = qobject_cast<Container*>( list_item->content() );

                    // Add the box art.
                    auto load_boxart = [data]() {
                        QFile image_file("data/" + data.toMap().value( "gameID" ).toString() + ".img");
                        image_file.open(QIODevice::ReadOnly);
                        return Image( image_file.readAll() );
                    };

                    auto *image_view = qobject_cast<ImageView*>( content->at(0) );
                    image_view->setImage( QFileInfo( "data/" + data.toMap().value( "gameID" ).toString() + ".img" ).exists() ? load_boxart() : nobox_icon );

                    // Add the title.
                    qobject_cast<Container*>( content->at(1) )->setVisible( data.toMap().value("settings").toMap().value("title").toBool() );
                    auto *label = qobject_cast<Label*>( qobject_cast<Container*>( content->at(1) )->at(0) );
                    label->setText( data.toMap().value( "title" ).toString() );

                    // Add context menu title
                    list_item->actionSetAt(0)->setTitle( data.toMap().value( "title" ).toString() );

                    // Map Context Menu Signals
                    rename_signal_map->setMapping( list_item->actionSetAt(0)->at(0), indexPath[0].toInt() ); //Rename
                    boxart_signal_map->setMapping( list_item->actionSetAt(0)->at(1), indexPath[0].toInt() ); //Set Game Cover Art
                    option_signal_map->setMapping( list_item->actionSetAt(0)->at(2), indexPath[0].toInt() ); //Settings
                    delete_signal_map->setMapping( list_item->actionSetAt(0)->at(3), indexPath[0].toInt() ); //Delete
                }
                else
                {
                    qobject_cast<ActivityIndicator*>( qobject_cast<Container*>( listItem )->at(0) )->start();
                }
            }

        public:
            GameItemProvider( QObject *parent ): ListItemProvider( parent ),

                nobox_icon( "asset:///ic_noboxart.png" )
            {
                bool connection;
                Q_UNUSED(connection);
                connection = connect( rename_signal_map, SIGNAL(mapped(int)), parent, SLOT(showRenameGamePrompt(int)) );
                Q_ASSERT(connection);
                connection = connect( boxart_signal_map, SIGNAL(mapped(int)), parent, SLOT(showSetGameArtPrompt(int)) );
                Q_ASSERT(connection);
                connection = connect( option_signal_map, SIGNAL(mapped(int)), parent, SLOT(showGameOptionPrompt(int)) );
                Q_ASSERT(connection);
                connection = connect( delete_signal_map, SIGNAL(mapped(int)), parent, SLOT(showDeleteGamePrompt(int)) );
                Q_ASSERT(connection);
            }
        };

        ListView *game_list_view = ListView::create().parent( game_library_view )
                                                     .dataModel( data_model )
                                                     .listItemProvider( new GameItemProvider( this ) )
                                                     .layout( GridListLayout::create().orientation( LayoutOrientation::TopToBottom ).parent( game_library_view ) );
        game_list_view->setListItemTypeMapper( new GameItemTypeMapper( game_list_view ) );

        game_library_content->add( game_list_view );
        game_library_content->setTopPadding( game_library_content->ui()->du(0.5f) );
        game_library_content->setBottomPadding( game_library_content->ui()->du(0.5f) );
        game_library_view->setTitleBar( segmented_bar );
        game_library_view->setContent( game_library_content );
        game_library_view->addAction( ActionItem::create().parent( game_library_view )
                                                          .title( "Import" )
                                                          .imageSource( QUrl("asset:///ic_add.png") )
                                                          .connect( SIGNAL( triggered() ), import_picker, SLOT( open() ) ), ActionBarPlacement::Signature );
        game_library_view->addAction( ActionItem::create().parent( game_library_view )
                                                          .title( "Play Game" ), ActionBarPlacement::InOverflow );


        /*
         *      Start building the saved states screen.
         * */
        // When we pass the ArrayDataModel to the ListView we only
        // want to make items if the data contains a list of "states."
        // To this end we look at the data map and, if it contains a
        // "states" key we create a new item. This ListItemTypeMapper
        // will act as a filter to accomplish this task.
        //
        class SaveItemTypeMapper : public ListItemTypeMapper, public QObject
        {
            QString itemType( const QVariant& data, const QVariantList& indexPath __attribute__((unused)) ) override
            {
                return data.toMap().contains( "states" ) ? "states" : "";
            }

        public:
            SaveItemTypeMapper(QObject* parent) : QObject(parent)
            {
            }
        };

        //
        //
        class SaveItems: public ListItemProvider
        {
            class SaveItem: public ListItemProvider
            {
                VisualNode *createItem( ListView *list, const QString &type __attribute__((unused)) ) override
                {
                    return ImageView::create("asset:///ic_noboxart.png").parent( list );
                }

                void updateItem( ListView           *list      __attribute__((unused)),
                                 VisualNode         *listItem  __attribute__((unused)),
                                 const QString      &type      __attribute__((unused)),
                                 const QVariantList &indexPath __attribute__((unused)),
                                 const QVariant     &data      __attribute__((unused)) ) override
                {
                }

            public:
                SaveItem( QObject *parent ): ListItemProvider( parent )
                {
                }
            };

            VisualNode *createItem( ListView *list, const QString &type ) override
            {
                if( type == "states" )
                {
                    return Container::create().parent( list )
                                              .add( Header::create().parent( list ) )
                                              .add( ListView::create().parent( list )
                                                                      .layout( StackListLayout::create().parent( list )
                                                                                                        .orientation( LayoutOrientation::LeftToRight ) )
                                                                      .preferredHeight( list->ui()->du( 30.0f ) )
                                                                      .top( list->ui()->du( 1.0f ) ) ) ;
                }
                else
                {
                    return Container::create().parent( list );
                }
            }

            void updateItem( ListView           *list      __attribute__((unused)),
                             VisualNode         *listItem,
                             const QString      &type      __attribute__((unused)),
                             const QVariantList &indexPath __attribute__((unused)),
                             const QVariant     &data ) override
            {
                if( type == "states" )
                {
                    auto *content = qobject_cast<Container*>( listItem );

                    // add a standard header
                    qobject_cast<Header*>( content->at(0) )->setTitle( data.toMap().value( "title" ).toString() );

                    // add ListView for saves
                    qobject_cast<ListView*>( content->at(1) )->setListItemProvider( new SaveItem( listItem ) );
                    qobject_cast<ListView*>( content->at(1) )->setDataModel( new ArrayDataModel( data.toMap().value( "states" ).toList(), listItem ) );
                }
            }

        public:
            SaveItems( QObject *parent ): ListItemProvider( parent )
            {
            }
        };

        ListView *save_list_view = ListView::create().dataModel( data_model )
                                                     .listItemProvider( new SaveItems( this ) )
                                                     .parent( saved_state_view );
        save_list_view->setListItemTypeMapper( new SaveItemTypeMapper( save_list_view ) );

        saved_state_content->add( save_list_view );
        saved_state_view->setContent( saved_state_content );

        bool connection;
        Q_UNUSED(connection);
        connection = connect( game_list_view, SIGNAL(triggered(QVariantList)), this, SLOT(onLibraryListTriggered(QVariantList)) );
        Q_ASSERT(connection);
        connection = connect( save_list_view, SIGNAL(triggered(QVariantList)), this, SLOT(onSavesListTriggered(QVariantList)) );
        Q_ASSERT(connection);


        /*
         *      Load data into model.
         * */
        loadDataBase();
    }


    Q_SLOT void onAboutToQuit()
    {
        if( !import_watcher.isNull() )
        {
            import_watcher->cancel();
            import_watcher->waitForFinished();

            QMutexLocker lk(&import_watcher_mutex);
        }
    }


public:
    GameLibraryUI(QObject *parent = nullptr): QObject(parent)
    {
        Application::instance()->setMenu( main_menu );
        Application::instance()->setScene( game_library_view );

        // Build UI when event handler is reached.
        QMetaObject::invokeMethod(this, "buildUI", Qt::QueuedConnection);

        boxart_picker->setViewMode( FilePickerViewMode::GridView );
        boxart_picker->setMode( FilePickerMode::Picker );
        boxart_picker->setType( FileType::Picture );
        boxart_picker->setTitle( "Select Image" );
        boxart_picker->setDirectories( QStringList("/accounts/1000/shared/photos") );

        import_picker->setViewMode( FilePickerViewMode::ListView );
        import_picker->setMode( FilePickerMode::PickerMultiple );
        import_picker->setType( FileType::Other );
        import_picker->setTitle( "Select ROM" );
        import_picker->setFilter( QStringList() << "*.bin" << "*.md" << "*.sms" << "*.gg" );
        import_picker->setDirectories( QStringList("/accounts/1000/shared/downloads") );

        bool connection;
        Q_UNUSED( connection );
        connection = connect( import_picker, SIGNAL(fileSelected(const QStringList&)), this, SLOT(importGames(const QStringList&)) );
        Q_ASSERT( connection );
        connection = connect( Application::instance(), SIGNAL(aboutToQuit()), this, SLOT(onAboutToQuit()) );
        Q_ASSERT( connection );
    }


    ~GameLibraryUI()
    {
    }
};
