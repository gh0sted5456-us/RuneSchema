#include "Misc/FileWatchWrapper.h"

namespace fs = std::filesystem;

namespace PS {
    FileWatchWrapper::FileWatchWrapper(const std::filesystem::path& path, const FilesystemUpdateCallback& callback)
    {
        m_fileWatcher = std::make_unique<efsw::FileWatcher>();
        m_updateListener = std::make_unique<UpdateListener>();
        m_updateListener->registerCallback(callback);

        m_fileWatchId = m_fileWatcher->addWatch(path.string(), m_updateListener.get(), true);
    }

    FileWatchWrapper::~FileWatchWrapper()
    {
        if (m_fileWatcher) {
            m_fileWatcher->removeWatch(m_fileWatchId);
            // Join before destroying the listener.
            m_fileWatcher.reset();
        }
        m_updateListener.reset();
    }

    void FileWatchWrapper::Watch()
    {
        if (!m_fileWatcher)
        {
            return;
        }

        m_fileWatcher->watch();
    }
}
