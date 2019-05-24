#include "GameLibraryUI.hpp"

#include <bb/cascades/Application>

using namespace bb::cascades;


Q_DECL_EXPORT int main(int argc, char **argv)
{
    Application app(argc, argv);
    GameLibraryUI game_library_ui;

    struct AtExit
    {
        ~AtExit()
        {
            /* "If the replaced scene (if one was set) is owned by the @c %Application
             *  it will be deleted, if not its ownership doesn't change. If it
             *  already has another parent the caller MUST ensure that setScene(0)
             *  is called before the scene object is deleted."
             */
            Application::instance()->setScene(0);
            Application::instance()->setCover(0);
        }
    } AtExit;


    return Application::exec();
}
