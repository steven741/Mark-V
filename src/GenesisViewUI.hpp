/*
 * GenesisViewUI.hpp
 *
 *  Created on: Dec 11, 2016
 *      Author: swatson
 */

#pragma once

extern "C" {
#ifndef Q_MOC_RUN
#include <shared.h>
#endif
}

#include <QMutex>
#include <QObject>

#include <screen/screen.h>
#include <sys/asoundlib.h>

#include <bb/device/DeviceInfo>
#include <bb/platform/HomeScreen>

#include <bb/cascades/Color>
#include <bb/cascades/Image>
#include <bb/cascades/Page>
#include <bb/cascades/Sheet>
#include <bb/cascades/Container>
#include <bb/cascades/DockLayout>
#include <bb/cascades/StackLayout>
#include <bb/cascades/ForeignWindowControl>
#include <bb/cascades/TitleBar>
#include <bb/cascades/ImageButton>
#include <bb/cascades/ActionItem>
#include <bb/cascades/KeyEvent>
#include <bb/cascades/KeyListener>
#include <bb/cascades/ScrollView>
#include <bb/cascades/FadeTransition>
#include <bb/cascades/StockCurve>
#include <bb/cascades/Application>

using namespace bb::cascades;


// TODO add save and load state support from toolbar.
// TODO add cheats support to toolbar.
// TODO add miracast support to toolbar.
// TODO add render filter to toolbar.
class Genesis: public QObject
{
    uint8_t brm_format[0x40] =
    {
        0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x00,0x00,0x00,0x00,0x40,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x53,0x45,0x47,0x41,0x5f,0x43,0x44,0x5f,0x52,0x4f,0x4d,0x00,0x01,0x00,0x00,0x00,
        0x52,0x41,0x4d,0x5f,0x43,0x41,0x52,0x54,0x52,0x49,0x44,0x47,0x45,0x5f,0x5f,0x5f
    };


public:
    static constexpr auto SOUND_FREQUENCY    = 44100;
    static constexpr auto SOUND_SAMPLES_SIZE = 2048;

    static constexpr auto VIDEO_WIDTH  = 320;
    static constexpr auto VIDEO_HEIGHT = 224;


    Genesis(const QString &rom, QObject *parent = nullptr): QObject(parent)
    {
        FILE *fp = NULL;

        error_init();
        set_config_defaults();

        /* mark all BIOS as unloaded */
        system_bios = 0;

        /* Genesis BOOT ROM support (2KB max) */
        memset(boot_rom, 0xFF, 0x800);
        fp = fopen(MD_BIOS, "rb");
        if (fp != NULL)
        {
            int i;

            /* read BOOT ROM */
            fread(boot_rom, 1, 0x800, fp);
            fclose(fp);

            /* check BOOT ROM */
            if (!memcmp((char *)(boot_rom + 0x120),"GENESIS OS", 10))
            {
                /* mark Genesis BIOS as loaded */
                system_bios = SYSTEM_MD;
            }

            /* Byteswap ROM */
            for (i=0; i<0x800; i+=2)
            {
                uint8 temp = boot_rom[i];
                boot_rom[i] = boot_rom[i+1];
                boot_rom[i+1] = temp;
            }
        }

        bitmap.width  = VIDEO_WIDTH;
        bitmap.height = VIDEO_HEIGHT;

        /* Load game file */
        if(!load_rom( rom.toAscii().constData() ))
        {
            fprintf(stderr, "failed to load rom.\n");
            fflush(stderr);
        }

        /* initialize system hardware */
        audio_init(SOUND_FREQUENCY, 0);
        system_init();

        /* Mega CD specific */
        if (system_hw == SYSTEM_MCD)
        {
           /* load internal backup RAM */
           fp = fopen("data/scd.brm", "rb");
           if (fp!=NULL)
           {
               fread(scd.bram, 0x2000, 1, fp);
               fclose(fp);
           }

           /* check if internal backup RAM is formatted */
           if (memcmp(scd.bram + 0x2000 - 0x20, brm_format + 0x20, 0x20))
           {
               /* clear internal backup RAM */
               memset(scd.bram, 0x00, 0x200);

               /* Internal Backup RAM size fields */
               brm_format[0x10] = brm_format[0x12] = brm_format[0x14] = brm_format[0x16] = 0x00;
               brm_format[0x11] = brm_format[0x13] = brm_format[0x15] = brm_format[0x17] = (sizeof(scd.bram) / 64) - 3;

               /* format internal backup RAM */
               memcpy(scd.bram + 0x2000 - 0x40, brm_format, 0x40);
           }

           /* load cartridge backup RAM */
           if (scd.cartridge.id)
           {
               fp = fopen("data/cart.brm", "rb");
               if (fp!=NULL)
               {
                   fread(scd.cartridge.area, scd.cartridge.mask + 1, 1, fp);
                   fclose(fp);
               }

               /* check if cartridge backup RAM is formatted */
               if (memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, brm_format + 0x20, 0x20))
               {
                   /* clear cartridge backup RAM */
                   memset(scd.cartridge.area, 0x00, scd.cartridge.mask + 1);

                   /* Cartridge Backup RAM size fields */
                   brm_format[0x10] = brm_format[0x12] = brm_format[0x14] = brm_format[0x16] = (((scd.cartridge.mask + 1) / 64) - 3) >> 8;
                   brm_format[0x11] = brm_format[0x13] = brm_format[0x15] = brm_format[0x17] = (((scd.cartridge.mask + 1) / 64) - 3) & 0xff;

                   /* format cartridge backup RAM */
                   memcpy(scd.cartridge.area + scd.cartridge.mask + 1 - sizeof(brm_format), brm_format, sizeof(brm_format));
               }
           }
        }

        if (sram.on)
        {
           /* load SRAM */
           fp = fopen("data/game.srm", "rb");
           if (fp!=NULL)
           {
               fread(sram.sram,0x10000,1, fp);
               fclose(fp);
           }
        }

        /* reset system hardware */
        system_reset();
    }


    ~Genesis()
    {
        FILE *fp = NULL;

        if (system_hw == SYSTEM_MCD)
        {
            /* save internal backup RAM (if formatted) */
            if (!memcmp(scd.bram + 0x2000 - 0x20, brm_format + 0x20, 0x20))
            {
                fp = fopen("data/scd.brm", "wb");
                if (fp!=NULL)
                {
                    fwrite(scd.bram, 0x2000, 1, fp);
                    fclose(fp);
                }
            }

            /* save cartridge backup RAM (if formatted) */
            if (scd.cartridge.id)
            {
                if (!memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, brm_format + 0x20, 0x20))
                {
                    fp = fopen("data/cart.brm", "wb");
                    if (fp!=NULL)
                    {
                        fwrite(scd.cartridge.area, scd.cartridge.mask + 1, 1, fp);
                        fclose(fp);
                    }
                }
            }
        }

        if (sram.on)
        {
            /* save SRAM */
            fp = fopen("data/game.srm", "wb");
            if (fp!=NULL)
            {
                fwrite(sram.sram,0x10000,1, fp);
                fclose(fp);
            }
        }

        audio_shutdown();
        error_shutdown();
    }
};



class GenesisViewUI: public QObject
{
    Q_OBJECT

    class ScreenThread: public QThread
    {
        int rect[4] = { 0, 0, Genesis::VIDEO_WIDTH, Genesis::VIDEO_HEIGHT };

        GenesisViewUI *instance;

        void run() override
        {
            while(instance->running)
            {
                QMutexLocker locker(&instance->sleep_video);

                screen_post_window(instance->screen_win, instance->screen_buf, 1, rect, SCREEN_WAIT_IDLE);
            }
        }

    public:
        ScreenThread(GenesisViewUI *parent): QThread(parent), instance(parent) {}
    };


    class AudioThread: public QThread
    {
        int16_t soundframe[Genesis::SOUND_SAMPLES_SIZE];

        GenesisViewUI *instance;

        void run() override
        {
            while(instance->running)
            {
                QMutexLocker locker(&instance->sleep_audio);

                if (system_hw == SYSTEM_MCD)
                {
                   system_frame_scd(0);
                }
                else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
                {
                   system_frame_gen(0);
                }
                else
                {
                   system_frame_sms(0);
                }

                snd_pcm_plugin_write(instance->pcm_handle, soundframe, audio_update(soundframe) * sizeof(int));
            }
        }

    public:
        AudioThread(GenesisViewUI *parent): QThread(parent), instance(parent) {}
    };

    bool paused  = false;
    bool toolbar = false;
    bool running = false;
    QMutex sleep_audio;
    QMutex sleep_video;

    QString BUTTON_A     = "i";
    QString BUTTON_B     = "o";
    QString BUTTON_C     = "p";
    QString BUTTON_START = " ";
    QString BUTTON_UP    = "w";
    QString BUTTON_DOWN  = "s";
    QString BUTTON_LEFT  = "a";
    QString BUTTON_RIGHT = "d";

    Genesis *genesis      = nullptr;
    QThread *audio_thread = new AudioThread(this);
    QThread *video_thread = new ScreenThread(this);

    Sheet *sheet = Sheet::create().parent(this)
                                  .peek(false)
                                  .connect(SIGNAL(closed()), this, SLOT(closeROM()));
    Container *opion_bar = Container::create().parent(this)
                                              .opacity(0.0f)
                                              .background( Color::Black )
                                              .horizontal( HorizontalAlignment::Fill )
                                              .add( ScrollView::create().parent(this)
                                                                        .vertical( VerticalAlignment::Top )
                                                                        .horizontal( HorizontalAlignment::Fill )
                                                                        .scrollRole( ScrollRole::Main )
                                                                        .scrollMode( ScrollMode::Horizontal )
                                                                        .content( Container::create().parent(this)
                                                                                                     .left( sheet->ui()->du(2.5f) )
                                                                                                     .right( sheet->ui()->du(2.5f) )
                                                                                                     .layout( StackLayout::create().parent(this).orientation( LayoutOrientation::LeftToRight ) )
                                                                                                     .add( ImageButton::create().parent(this)
                                                                                                                                .defaultImage( QUrl("asset:///ic_save.png") )
                                                                                                                                .vertical( VerticalAlignment::Center )
                                                                                                                                .horizontal( HorizontalAlignment::Center )
                                                                                                                                .preferredSize( sheet->ui()->du(11.0f), sheet->ui()->du(11.0f) ) )
                                                                                                     .add( ImageButton::create().parent(this)
                                                                                                                                .defaultImage( QUrl("asset:///ic_load.png") )
                                                                                                                                .vertical( VerticalAlignment::Center )
                                                                                                                                .horizontal( HorizontalAlignment::Center )
                                                                                                                                .preferredSize( sheet->ui()->du(11.0f), sheet->ui()->du(11.0f) ) ) ) );

    /* Screen API Handles */
    screen_context_t screen_ctx = nullptr;
    screen_window_t  screen_win = nullptr;
    screen_buffer_t  screen_buf = nullptr;

    /* QSA Handles */
    snd_pcm_t *pcm_handle = nullptr;

    bb::device::DeviceInfo device_info;
    bb::platform::HomeScreen home_screen;


    Q_SLOT void keyPressed(bb::cascades::KeyEvent *event)
    {
        const QString &key = event->unicode();

        if( key == BUTTON_A )     input.pad[0] |= INPUT_A; else
        if( key == BUTTON_B )     input.pad[0] |= INPUT_B; else
        if( key == BUTTON_C )     input.pad[0] |= INPUT_C; else
        if( key == BUTTON_START ) input.pad[0] |= INPUT_START;
        if( key == BUTTON_UP )    input.pad[0] |= INPUT_UP; else
        if( key == BUTTON_DOWN )  input.pad[0] |= INPUT_DOWN;
        if( key == BUTTON_LEFT )  input.pad[0] |= INPUT_LEFT; else
        if( key == BUTTON_RIGHT ) input.pad[0] |= INPUT_RIGHT;
    }


    Q_SLOT void keyReleased(bb::cascades::KeyEvent *event)
    {
        const QString &key = event->unicode();

        if( key == BUTTON_A )     input.pad[0] &= ~INPUT_A; else
        if( key == BUTTON_B )     input.pad[0] &= ~INPUT_B; else
        if( key == BUTTON_C )     input.pad[0] &= ~INPUT_C; else
        if( key == BUTTON_START ) input.pad[0] &= ~INPUT_START;
        if( key == BUTTON_UP )    input.pad[0] &= ~INPUT_UP; else
        if( key == BUTTON_DOWN )  input.pad[0] &= ~INPUT_DOWN;
        if( key == BUTTON_LEFT )  input.pad[0] &= ~INPUT_LEFT; else
        if( key == BUTTON_RIGHT ) input.pad[0] &= ~INPUT_RIGHT;
    }


    Q_SLOT void addBar()
    {
        Container *content = qobject_cast<Container*>( qobject_cast<Page*>( sheet->content() )->content() );

        content->add( opion_bar );
        toolbar = true;
    }


    Q_SLOT void removeBar()
    {
        Container *content = qobject_cast<Container*>( qobject_cast<Page*>( sheet->content() )->content() );

        content->remove( opion_bar );
        toolbar = false;
    }


    Q_SLOT void toggleOptionBar()
    {
        if(toolbar)
        {
            ((FadeTransition*)FadeTransition::create(opion_bar).parent( opion_bar )
                                                               .connect( SIGNAL(ended()), this, SLOT(resume()) )
                                                               .connect( SIGNAL(ended()), this, SLOT(removeBar()) )
                                                               .autoDeleted(true)
                                                               .easingCurve(StockCurve::Linear)
                                                               .from(0.80f)
                                                               .to(0.0f)
                                                               .duration(120))->play();
        }
        else
        {
            pause();
            addBar();
            ((FadeTransition*)FadeTransition::create(opion_bar).parent( opion_bar )
                                                               .autoDeleted(true)
                                                               .easingCurve(StockCurve::Linear)
                                                               .from(0.0f)
                                                               .to(0.80f)
                                                               .duration(120))->play();
        }
    }


    Q_SLOT void onThumbnail()
    {
        pause();
    }


    Q_SLOT void onFullscreen()
    {
        if(!toolbar)
            resume();
    }


    Q_SLOT void onLockStateChanged(bb::platform::DeviceLockState::Type type)
    {
        if(type == bb::platform::DeviceLockState::Unlocked && Application::instance()->isFullscreen() && !toolbar) resume();
    }


    Q_SLOT void onActivityStateChanged(bb::device::UserActivityState::Type type)
    {
        if(type == bb::device::UserActivityState::Inactive) pause();
    }


public:
    //
    //
    GenesisViewUI(QObject *parent = nullptr): QObject( parent )
    {
    }


    //
    //
    ~GenesisViewUI()
    {
        closeROM();
    }


    bool isPaused() { return paused; }
    bool isRunning() { return running && screen_ctx; }


    //
    //
    Q_SLOT void openROM(const QVariantMap &game)
    {
        if(!running)
        {
            /* *
             *      Cascades UI
             */
            Page *root = Page::create().parent(this);
            Container *content = Container::create().parent(root)
                                                    .layout( DockLayout::create().parent(root) );
            ForeignWindowControl *emulator_view = ForeignWindowControl::create().parent(content)
                                                                                .preferredSize( 768.0f, 768.0f *Genesis::VIDEO_HEIGHT/Genesis::VIDEO_WIDTH )
                                                                                .windowId("genesis_view")
                                                                                .updatedProperties( WindowProperty::Size |
                                                                                                    WindowProperty::Position |
                                                                                                    WindowProperty::Visible );
            content->add( emulator_view );
            root->setContent( content );
            root->setTitleBar( TitleBar::create().parent( root )
                                                 .title( game.value("title").toString() )
                                                 .acceptAction( ActionItem::create().parent(root).title("Options").onTriggered(this, SLOT(toggleOptionBar()) ) )
                                                 .dismissAction( ActionItem::create().parent(root).title("Close").onTriggered(sheet, SLOT(close())) ) );
            root->addKeyListener( KeyListener::create().parent( root )
                                                       .onKeyPressed( this, SLOT(keyPressed(bb::cascades::KeyEvent*)) )
                                                       .onKeyReleased( this, SLOT(keyReleased(bb::cascades::KeyEvent*)) ) );
            sheet->setContent( root );
            sheet->open();

            bool connection;
            connection = connect( sheet, SIGNAL(closed()), root, SLOT(deleteLater()) );
            Q_ASSERT( connection );
            connection = connect( Application::instance(), SIGNAL(thumbnail()), this, SLOT(onThumbnail()) );
            Q_ASSERT( connection );
            connection = connect( Application::instance(), SIGNAL(fullscreen()), this, SLOT(onFullscreen()) );
            Q_ASSERT( connection );
            connection = connect( &device_info, SIGNAL(activityStateChanged(bb::device::UserActivityState::Type)), this, SLOT(onActivityStateChanged(bb::device::UserActivityState::Type)) );
            Q_ASSERT( connection );
            connection = connect( &home_screen, SIGNAL(lockStateChanged(bb::platform::DeviceLockState::Type)), this, SLOT(onLockStateChanged(bb::platform::DeviceLockState::Type)) );
            Q_ASSERT( connection );
            connection = connect( emulator_view, SIGNAL(windowAttached(screen_window_t, const QString&, const QString&)), audio_thread, SLOT(start()) );
            Q_ASSERT( connection );
            connection = connect( emulator_view, SIGNAL(windowAttached(screen_window_t, const QString&, const QString&)), video_thread, SLOT(start()) );
            Q_ASSERT( connection );
            connection = connect( emulator_view, SIGNAL(windowAttached(screen_window_t, const QString&, const QString&)), this, SIGNAL(opened()) );
            Q_ASSERT( connection );
            Q_UNUSED( connection );



            /**
             *      QNX Sound Architecture
             *
             * http://www.qnx.com/developers/docs/6.4.0/neutrino/audio/architecture.html
             * http://www.qnx.com/developers/docs/6.4.0/neutrino/audio/pcm.html
             * http://www.qnx.com/developers/docs/6.4.0/neutrino/audio/mixer.html
             *
             */
            snd_pcm_channel_params_t pp;

            memset(&pp, 0, sizeof(snd_pcm_channel_params_t));
            pp.mode       = SND_PCM_MODE_BLOCK;
            pp.channel    = SND_PCM_CHANNEL_PLAYBACK;
            pp.start_mode = SND_PCM_START_FULL;
            pp.stop_mode  = SND_PCM_STOP_ROLLOVER_RESET;

            pp.format.interleave = 1;
            pp.format.rate       = Genesis::SOUND_FREQUENCY;
            pp.format.voices     = 2;
            pp.format.format     = SND_PCM_SFMT_S16_LE;

            pp.buf.block.frags_max = 5;
            pp.buf.block.frags_min = 1;
            pp.buf.block.frag_size = Genesis::SOUND_SAMPLES_SIZE;

            int snd_errno = -1;

            snd_errno = snd_pcm_open_name(&pcm_handle, "pcmPreferred", SND_PCM_OPEN_PLAYBACK);
            if( snd_errno < 0 )
            {
                fprintf( stderr, "snd_pcm_open_name failed: %s\n", snd_strerror(snd_errno) );
            }

            snd_errno = snd_pcm_plugin_params(pcm_handle, &pp);
            if( snd_errno < 0 )
            {
                fprintf( stderr, "snd_pcm_plugin_params failed: %s\n", snd_strerror(snd_errno) );
            }

            snd_errno = snd_pcm_plugin_prepare(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
            if( snd_errno < 0 )
            {
                fprintf( stderr, "snd_pcm_plugin_prepare failed: %s\n", snd_strerror(snd_errno) );
            }


            /**
             *      QNX Screen API
             *
             *      http://www.qnx.com/developers/docs/660/index.jsp?topic=%2Fcom.qnx.doc.screen%2Ftopic%2Fmanual%2Fcscreen_about.html
             *
             */
            const QByteArray &id = emulator_view->windowId().toAscii();
            const QByteArray &group = emulator_view->windowGroup().toAscii();

            /* Create Screen Context */
            if( screen_create_context(&screen_ctx, SCREEN_APPLICATION_CONTEXT) ) {
                perror("screen_create_context");
            }

            /* Create Screen Window */
            if( screen_create_window_type(&screen_win, screen_ctx, SCREEN_CHILD_WINDOW) ) {
                perror("screen_create_window_type");
            }

            /* Create Screen Buffer */
            if( screen_create_window_buffers(screen_win, 1) ) {
                perror("screen_create_window_buffers");
            }

            /* Attach Window to ForignWindowView */
            if( screen_join_window_group(screen_win, group.constData()) ) {
                perror("screen_join_window_group");
            }

            if( screen_set_window_property_cv(screen_win, SCREEN_PROPERTY_ID_STRING, id.length(), id.constData()) ) {
                perror("screen_set_window_property_cv");
            }

#ifdef QT_DEBUG
            int debug = SCREEN_DEBUG_STATISTICS;
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_DEBUG, &debug) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_DEBUG)");
            }
#endif

            int z = -5;
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &z) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_ZORDER)");
            }

            int interval = 1;
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SWAP_INTERVAL, &interval) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_SWAP_INTERVAL)");
            }

            int idle_mode = SCREEN_IDLE_MODE_KEEP_AWAKE;
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_IDLE_MODE, &idle_mode) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_IDLE_MODE)");
            }

            int usage = SCREEN_USAGE_WRITE;
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)");
            }

            int format = SCREEN_FORMAT_RGB565;
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_FORMAT, &format) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)");
            }

            int scale = 16;
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SCALE_FACTOR, &scale) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_SCALE_FACTOR)");
            }

            int scale_quality = SCREEN_QUALITY_FASTEST;
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SCALE_QUALITY, &scale_quality) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_SCALE_QUALITY)");
            }

            int dims[2] = { Genesis::VIDEO_WIDTH, Genesis::VIDEO_HEIGHT };
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SOURCE_SIZE, dims) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_SOURCE_SIZE)");
            }

            int rect[2] = { Genesis::VIDEO_WIDTH, Genesis::VIDEO_HEIGHT };
            if( screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, rect) ) {
                perror("screen_set_window_property_iv(SCREEN_PROPERTY_BUFFER_SIZE)");
            }

            /* Get Screen Buffer Attributes */
            if( screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf) ) {
                perror("screen_get_window_property_pv(SCREEN_PROPERTY_RENDER_BUFFERS)");
            }

            if( screen_get_buffer_property_iv(screen_buf, SCREEN_PROPERTY_STRIDE, &bitmap.pitch) ) {
                perror("screen_get_buffer_property_pv(SCREEN_PROPERTY_POINTER)");
            }

            if( screen_get_buffer_property_pv(screen_buf, SCREEN_PROPERTY_POINTER, (void **)&bitmap.data) ) {
                perror("screen_get_buffer_property_iv(SCREEN_PROPERTY_STRIDE)");
            }


            /**
             *      Open the ROM.
             */
            genesis = new Genesis("data/"+ game.value("gameID").toString() +".bin", this);

            paused  = false;
            toolbar = false;
            running = true;
        }
    }


    //
    //
    Q_SLOT void closeROM()
    {
        if(running)
        {
            if(paused || toolbar)
            {
                sleep_audio.unlock();
                sleep_video.unlock();
            }

            paused  = false;
            toolbar = false;
            running = false;

            audio_thread->wait();
            video_thread->wait();
            opion_bar->setOpacity(0.0f);

            /* Free QSA resource. */
            snd_pcm_close(pcm_handle);

            /* Free Screen API resources */
            screen_destroy_buffer(screen_buf);
            screen_destroy_window(screen_win);
            screen_destroy_context(screen_ctx);

            bool connection;
            connection = disconnect( Application::instance(), SIGNAL(thumbnail()), this, SLOT(onThumbnail()) );
            Q_ASSERT( connection );
            connection = disconnect( Application::instance(), SIGNAL(fullscreen()), this, SLOT(onFullscreen()) );
            Q_ASSERT( connection );
            connection = disconnect( &device_info, SIGNAL(activityStateChanged(bb::device::UserActivityState::Type)), this, SLOT(onActivityStateChanged(bb::device::UserActivityState::Type)) );
            Q_ASSERT( connection );
            connection = disconnect( &home_screen, SIGNAL(lockStateChanged(bb::platform::DeviceLockState::Type)), this, SLOT(onLockStateChanged(bb::platform::DeviceLockState::Type)) );
            Q_ASSERT( connection );
            Q_UNUSED( connection );

            delete genesis;
            screen_ctx = nullptr;
            screen_win = nullptr;
            screen_buf = nullptr;
            emit closed("");
        }
    }


    //
    //
    Q_SLOT void pause()
    {
        if(!paused && running)
        {
            paused = true;
            sleep_video.lock();
            sleep_audio.lock();

            snd_pcm_channel_pause(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
        }
    }


    //
    //
    Q_SLOT void resume()
    {
        if(paused && running)
        {
            paused = false;
            sleep_audio.unlock();
            sleep_video.unlock();

            snd_pcm_channel_resume(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
        }
    }


    //
    //
    Q_SLOT void loadState(const QString &file)
    {
        Q_UNUSED(file);
    }


    //
    //
    Q_SLOT void saveState(const QString &dir, const QString &name)
    {
        emit stateSaved(dir+name);
    }


    //
    //
    Q_SLOT void saveScreenshot(const QString &dir, const QString &name)
    {
        emit screenshotSaved(dir+name);
    }


Q_SIGNALS:
    void opened();
    void closed(const QString &file);
    void stateSaved(const QString &file);
    void screenshotSaved(const QString &file);
};
