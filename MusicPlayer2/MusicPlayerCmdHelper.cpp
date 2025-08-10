﻿#include "stdafx.h"
#include "MusicPlayerCmdHelper.h"
#include "InternetCommon.h"
#include "Player.h"
#include "MusicPlayer2.h"
#include "MusicPlayerDlg.h"
#include "InputDlg.h"
#include "Playlist.h"
#include "AddToPlaylistDlg.h"
#include "AudioCommon.h"
#include "COSUPlayerHelper.h"
#include "SongDataManager.h"
#include "SelectItemDlg.h"
#include "CommonDialogMgr.h"
#include "FilterHelper.h"
#include "UiMediaLibItemMgr.h"
#include "SongInfoHelper.h"
#include "CRecentList.h"
#include "SongMultiVersion.h"

CMusicPlayerCmdHelper::CMusicPlayerCmdHelper(CWnd* pOwner)
    : m_pOwner(pOwner)
{
}

CMusicPlayerCmdHelper::~CMusicPlayerCmdHelper()
{
}

void CMusicPlayerCmdHelper::VeiwOnline(SongInfo& song)
{
    //查找歌曲并获取最佳匹配项的歌曲ID
    std::wstring song_id;
    CSongDataManager::GetInstance().GetSongID(song, song_id);  // 从媒体库读取id
    song.SetSongId(song_id);
    if (song_id.empty())		//如果没有获取过ID，则获取一次ID
    {
        song_id = theApp.GetLyricDownload()->SearchSongAndGetMatched(song.title, song.artist, song.album, song.GetFileName()).id;
        song.SetSongId(song_id);
        CSongDataManager::GetInstance().SetSongID(song, song_id);  // 与媒体库同步
    }

    if (song_id.empty())
        return;
    //获取网易云音乐中该歌曲的在线接听网址
    wstring song_url{ theApp.GetLyricDownload()->GetOnlineUrl(song.GetSongId()) };

    //打开超链接
    ShellExecute(NULL, _T("open"), song_url.c_str(), NULL, NULL, SW_SHOW);

}

UINT CMusicPlayerCmdHelper::ViewOnlineThreadFunc(LPVOID lpParam)
{
    SongInfo* song = (SongInfo*)(lpParam);
    if (song == nullptr || song->IsEmpty())
        return 0;
    CCommon::SetThreadLanguageList(theApp.m_str_table.GetLanguageTag());
    CMusicPlayerCmdHelper helper;
    helper.VeiwOnline(*song);
    return 0;
}

void CMusicPlayerCmdHelper::FormatConvert(const std::vector<SongInfo>& songs)
{
    if (!theApp.m_format_convert_dialog_exit)
        return;
    CMusicPlayerDlg* pPlayerDlg = CMusicPlayerDlg::GetInstance();
    if (pPlayerDlg == nullptr)
        return;

    CCommon::DeleteModelessDialog(pPlayerDlg->m_pFormatConvertDlg);

    if (CPlayer::GetInstance().IsMciCore())
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_MCI_NO_THIS_FUNCTION_WARNING");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
        return;
    } else if (CPlayer::GetInstance().IsFfmpegCore()) {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_FFMPEG_NO_THIS_FUNCTION_WARNING");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
        return;
    }

    pPlayerDlg->m_pFormatConvertDlg = new CFormatConvertDlg(songs, GetOwner());
    pPlayerDlg->m_pFormatConvertDlg->Create(IDD_FORMAT_CONVERT_DIALOG);
    pPlayerDlg->m_pFormatConvertDlg->ShowWindow(SW_SHOW);
}

bool CMusicPlayerCmdHelper::OnAddToNewPlaylist(std::function<void(std::vector<SongInfo>&)> get_song_list, std::wstring& playlist_path, const std::wstring& default_name /*= L""*/)
{
    CInputDlg imput_dlg(GetOwner());
    imput_dlg.SetTitle(theApp.m_str_table.LoadText(L"TITLE_NEW_PLAYLIST").c_str());
    imput_dlg.SetInfoText(theApp.m_str_table.LoadText(L"TXT_NEW_PLAYLIST_INPUT_PLAYLIST_NAME").c_str());
    imput_dlg.SetEditText(default_name.c_str());
    if (imput_dlg.DoModal() == IDOK)
    {
        wstring playlist_name = imput_dlg.GetEditText().GetString();
        if (playlist_name.empty())
        {
            const wstring& info = theApp.m_str_table.LoadText(L"MSG_PLAYLIST_NAME_EMPTY_WARNING");
            GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
            return false;
        }
        if (!CCommon::IsFileNameValid(playlist_name))
        {
            const wstring& info = theApp.m_str_table.LoadText(L"MSG_FILE_NAME_INVALID_WARNING");
            GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
            return false;
        }

        //获取选中的曲目的路径
        std::vector<SongInfo> selected_item_path;
        get_song_list(selected_item_path);
        
        playlist_path = theApp.m_playlist_dir + playlist_name + PLAYLIST_EXTENSION;
        // 创建新的空播放列表
        if (!CRecentList::Instance().AddNewItem(ListItem{ LT_PLAYLIST, playlist_path }))
        {
            //播放列表已存在，询问是否要添加到已存在的播放列表
            wstring info = theApp.m_str_table.LoadTextFormat(L"MSG_PLAYLIST_EXIST_INQUIRY", { playlist_name, selected_item_path.size()});
            if (GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONQUESTION | MB_YESNO) != IDYES)
            {
                return false;
            }
        }

        CMusicPlayerDlg* pPlayerDlg = CMusicPlayerDlg::GetInstance();

        CPlaylistFile playlist;
        playlist.LoadFromFile(playlist_path);
        int rtn = playlist.AddSongsToPlaylist(selected_item_path, theApp.m_media_lib_setting_data.insert_begin_of_playlist);
        if (rtn > 0)
        {
            //显示添加成功提示
            CPlayerUIBase* ui = pPlayerDlg->GetCurrentUi();
            if (ui != nullptr)
            {
                wstring playlist_display_name = ListItem(LT_PLAYLIST, playlist_path).GetDisplayName();
                const wstring& info = theApp.m_str_table.LoadTextFormat(L"MSG_ADD_TO_PLAYLIST_SUCCEED", { rtn, playlist_display_name });
                ui->ShowUiTipInfo(info);
            }
        }
        else
        {
            const wstring& info = theApp.m_str_table.LoadText(L"MSG_FILE_EXIST_IN_PLAYLIST");
            pPlayerDlg->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
            return false;
        }
        playlist.SaveToFile(playlist_path);
        // 此方法需要自行更新“添加到播放列表”菜单和媒体库标签页
        theApp.m_pMainWnd->SendMessage(WM_INIT_ADD_TO_MENU);
        RefreshMediaTabData(ML_PLAYLIST);

        return true;
    }
    return false;
}

bool CMusicPlayerCmdHelper::OnAddToPlaylistCommand(std::function<void(std::vector<SongInfo>&)> get_song_list, DWORD command)
{
    //响应播放列表右键菜单中的“添加到播放列表”
    if ((command >= ID_ADD_TO_DEFAULT_PLAYLIST && command <= ID_ADD_TO_MY_FAVOURITE + ADD_TO_PLAYLIST_MAX_SIZE) || command == ID_ADD_TO_OTHER_PLAYLIST)
    {
        //获取选中的曲目
        std::vector<SongInfo> selected_item_path;
        get_song_list(selected_item_path);

        if (command == ID_ADD_TO_OTHER_PLAYLIST)
        {
            CAddToPlaylistDlg dlg;
            if (dlg.DoModal() == IDOK)
            {
                wstring playlist_path = theApp.m_playlist_dir + dlg.GetPlaylistSelected() + PLAYLIST_EXTENSION;
                if (CCommon::FileExist(playlist_path))
                {
                    AddToPlaylist(selected_item_path, playlist_path);
                }
            }
        }
        else if (command == ID_ADD_TO_DEFAULT_PLAYLIST)      //添加到默认播放列表
        {
            std::wstring default_playlist_path = CRecentList::Instance().GetSpecPlaylist(CRecentList::PT_DEFAULT).path;
            AddToPlaylist(selected_item_path, default_playlist_path);
        }
        else if (command == ID_ADD_TO_MY_FAVOURITE)      //添加到“我喜欢”播放列表
        {
            std::wstring favourite_playlist_path = CRecentList::Instance().GetSpecPlaylist(CRecentList::PT_FAVOURITE).path;
            AddToPlaylist(selected_item_path, favourite_playlist_path);

            //添加到“我喜欢”播放列表后，为添加的项目设置favourite标记
            for (const auto& item : selected_item_path)
            {
                auto& cur_playlist{ CPlayer::GetInstance().GetPlayList() };
                auto iter = std::find(cur_playlist.begin(), cur_playlist.end(), item);
                if (iter != cur_playlist.end())
                    iter->is_favourite = true;
            }
            //for (auto i : m_items_selected)
            //{
            //    if (i >= 0 && i < CPlayer::GetInstance().GetSongNum())
            //    {
            //        CPlayer::GetInstance().GetPlayList()[i].is_favourite = true;
            //    }
            //}

            CUiMyFavouriteItemMgr::Instance().UpdateMyFavourite();
        }
        else        //添加到选中的播放列表
        {
            int index = command - ID_ADD_TO_MY_FAVOURITE - 1;
            ListItem list_item{};

            CMusicPlayerDlg* pPlayerDlg = CMusicPlayerDlg::GetInstance();
            if (index >= 0 && index < static_cast<int>(pPlayerDlg->m_playlist_cache.size()))
                list_item = pPlayerDlg->m_playlist_cache.at(index);

            if (CCommon::FileExist(list_item.path))
            {
                AddToPlaylist(selected_item_path, list_item.path);
            }
            else
            {
                wstring info = theApp.m_str_table.LoadTextFormat(L"MSG_PLAYLIST_ADD_SONGS_FAILED",
                    { CFilePathHelper(list_item.path).GetFileNameWithoutExtension() });
                GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
            }
        }
        return true;
    }
    return false;
}

bool CMusicPlayerCmdHelper::DeleteSongsFromDisk(const std::vector<SongInfo>& files)
{
    if (theApp.m_media_lib_setting_data.disable_delete_from_disk)
        return false;

    if (files.empty())
        return false;

    wstring info;
    if (files.size() == 1)
        info = theApp.m_str_table.LoadTextFormat(L"MSG_DELETE_SINGLE_FILE_INQUIRY", { files.front().file_path });
    else
        info = theApp.m_str_table.LoadTextFormat(L"MSG_DELETE_SEL_AUDIO_FILE_INQUIRY", { files.size() });
    if (GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OKCANCEL) != IDOK)
        return false;

    vector<wstring> delected_files;
    for (const auto& song : files)
    {
        if (!song.is_cue && !COSUPlayerHelper::IsOsuFile(song.file_path))
            delected_files.push_back(song.file_path);

    }
    if (delected_files.empty())
        return false;

    wstring current_file = CPlayer::GetInstance().GetCurrentSongInfo().file_path;
    //如何要删除的文件是正在播放的文件，则先停止播放
    if (CCommon::IsItemInVector(delected_files, current_file))
    {
        CPlayer::GetInstance().GetPlayStatusMutex().lock();
        CPlayer::GetInstance().MusicControl(Command::STOP);
        CPlayer::GetInstance().MusicControl(Command::CLOSE);
        CPlayer::GetInstance().GetPlayStatusMutex().unlock();
    }
    int rtn{};
    rtn = CommonDialogMgr::DeleteFiles(GetOwner()->m_hWnd, delected_files);
    if (rtn == 0)
    {
        //如果文件删除成功，则从歌曲数据中删除
        for (const auto& file : delected_files)
        {
            CSongDataManager::GetInstance().RemoveItem(file);
        }
        //文件删除后同时删除和文件同名的图片文件和歌词文件
        for (auto& file : delected_files)
        {
            CFilePathHelper file_path(file);
            file = file_path.ReplaceFileExtension(L"jpg").c_str();
        }
        CommonDialogMgr::DeleteFiles(GetOwner()->m_hWnd, delected_files);
        for (const wstring& ext : CLyrics::m_surpported_lyric)      // 删除所有后缀的歌词
        {
            for (auto& file : delected_files)
            {
                CFilePathHelper file_path(file);
                file = file_path.ReplaceFileExtension(ext.c_str()).c_str();
            }
            CommonDialogMgr::DeleteFiles(GetOwner()->m_hWnd, delected_files);
        }
    }
    else if (rtn == 1223)	//如果在弹出的对话框中点击“取消”则返回值为1223
    {
        return false;
    }
    else
    {
        GetOwner()->MessageBox(theApp.m_str_table.LoadText(L"MSG_DELETE_FILE_FAILED").c_str(), NULL, MB_ICONWARNING);
        return false;
    }
    return true;
}

void CMusicPlayerCmdHelper::SearchLyricFiles(const wstring& lyric_name, const wstring& cur_dir, std::vector<std::wstring>& result, bool fuzzy_match)
{
    result.clear();

    // 查找歌词文件名和搜索关键词完全匹配的歌词
    CFilePathHelper lyric_path{ cur_dir + lyric_name + L".lrc"};     // 得到路径+文件名的字符串，预先加扩展名防止之后ReplaceFileExtension替换误伤

    // 按顺序列出所有需要查找的目录
    vector<wstring> path_list{ cur_dir };
    if (CCommon::FolderExist(theApp.m_lyric_setting_data.AbsoluteLyricPath()))
    {
        path_list.push_back(theApp.m_lyric_setting_data.AbsoluteLyricPath());
    }

    // 完全匹配搜索，使用CCommon::FileExist的快速查找
    for (const wstring& pa : path_list)
    {
        lyric_path.SetFilePath(pa + lyric_path.GetFileName());
        for (const wstring& ext : CLyrics::m_surpported_lyric)
        {
            lyric_path.ReplaceFileExtension(ext.c_str());
            if (CCommon::FileExist(lyric_path.GetFilePath()))
                result.push_back(lyric_path.GetFilePath());
        }
    }

    // 列出所有搜索路径内的歌词文件
    vector<vector<wstring>> path_lyrics;
    path_lyrics.resize(path_list.size());
    for (int i{}; i < static_cast<int>(path_list.size()); ++i)
    {
        CAudioCommon::GetLyricFiles(path_list[i], path_lyrics[i]);
    }

    //将查找的歌词名称拆分成若干个子字符串
    vector<wstring> key_words;
    CCommon::StringSplit(lyric_name, L'-', key_words);
    for (auto& key_word : key_words)
    {
        CCommon::StringNormalize(key_word);
        CCommon::FileNameNormalize(key_word);
    }

    // 判断一个字符串是否匹配key_words中任意一个关键字（根据是否模糊搜索使用不同方式）
    auto isMatched = [&](const wstring& str, const vector<wstring>& key_words)
    {
        if (fuzzy_match)
        {
            //如果是部分匹配，则只需要有一个关键字匹配，就返回true
            for (const auto& key_word : key_words)
            {
                if (CCommon::StringNatchWholeWord(str, key_word) != -1)
                    return true;
            }
            return false;
        }
        else
        {
            //否则，需要所有关键字都匹配才返回true
            for (const auto& key_word : key_words)
            {
                if (CCommon::StringNatchWholeWord(str, key_word) == -1)
                    return false;
            }
            return true;
        }
    };

    for (int i{}; i < static_cast<int>(path_lyrics.size()); ++i)
    {
        const vector<wstring>& cur_path = path_lyrics[i];
        for (const wstring& str : cur_path)
        {
            if (isMatched(str, key_words))
            {
                wstring matched_lyric = path_list[i] + str;
                if (!CCommon::IsItemInVector(result, matched_lyric))
                    result.push_back(matched_lyric);
            }
        }
    }
}

std::wstring CMusicPlayerCmdHelper::SearchLyricFile(const SongInfo& song, bool fuzzy_match)
{
    if (song.GetFileName().size() < 3) return wstring();

    // cue与osu文件禁止按音频文件名搜索，无视模糊搜索，优先搜索“艺术家 - 标题”
    bool find_org_name{ !song.is_cue && !COSUPlayerHelper::IsOsuFile(song.file_path) };
    CFilePathHelper lyric_path{ song.file_path };
    if (!find_org_name)
    {
        wstring ar_ti{ song.artist + L" - " + song.title + L".lrc"};    // 预先加扩展名防止之后ReplaceFileExtension替换误伤
        CCommon::FileNameNormalize(ar_ti);
        lyric_path.SetFilePath(lyric_path.GetDir() + ar_ti);
    }

    // 按顺序列出所有需要查找的目录
    vector<wstring> path_list{ lyric_path.GetDir() };
    if (CCommon::FolderExist(theApp.m_lyric_setting_data.AbsoluteLyricPath()))
    {
        path_list.push_back(theApp.m_lyric_setting_data.AbsoluteLyricPath());
    }

    // 完全匹配搜索，使用CCommon::FileExist的快速查找
    for (const wstring& pa : path_list)
    {
        lyric_path.SetFilePath(pa + lyric_path.GetFileName());
        for (const wstring& ext : CLyrics::m_surpported_lyric)
        {
            lyric_path.ReplaceFileExtension(ext.c_str());
            if (CCommon::FileExist(lyric_path.GetFilePath()))
                return lyric_path.GetFilePath();
        }
    }
    // 进行模糊查找，由于需要列出所有歌词文件所以较慢
    if (fuzzy_match)
    {
        // 列出所有搜索路径内的歌词文件
        vector<vector<wstring>> path_lyrics;
        path_lyrics.resize(path_list.size());
        for (int i{}; i < static_cast<int>(path_list.size()); ++i)
        {
            CAudioCommon::GetLyricFiles(path_list[i], path_lyrics[i]);
        }
        wstring matched_lyric;      // 匹配的歌词的路径
        wstring title{ song.title }, artist{ song.artist };
        CCommon::FileNameNormalize(title);
        CCommon::FileNameNormalize(artist);
        // 先寻找歌词文件中同时包含歌曲标题和艺术家的歌词文件
        for (int i{}; i < static_cast<int>(path_lyrics.size()); ++i)
        {
            const vector<wstring>& cur_path = path_lyrics[i];
            for (const wstring& str : cur_path)
            {
                if (CCommon::StringNatchWholeWord(str, artist) != -1 && CCommon::StringNatchWholeWord(str, title) != -1)
                {
                    matched_lyric = path_list[i] + str;
                    return matched_lyric;
                }
            }
        }
        // 没有找到的话就寻找歌词文件中只包含歌曲标题的歌词文件
        for (int i{}; i < static_cast<int>(path_lyrics.size()); ++i)
        {
            const vector<wstring>& cur_path = path_lyrics[i];
            for (const wstring& str : cur_path)
            {
                if (CCommon::StringNatchWholeWord(str, title) != -1)
                {
                    matched_lyric = path_list[i] + str;
                    return matched_lyric;
                }
            }
        }
    }
    return wstring();
}

std::wstring CMusicPlayerCmdHelper::SearchAlbumCover(const SongInfo& song)
{
    wstring album_cover_path;
    if (COSUPlayerHelper::IsOsuFile(song.file_path))
    {
        album_cover_path = COSUPlayerHelper::GetAlbumCover(song.file_path);
        if (album_cover_path.empty())
            album_cover_path = theApp.m_nc_setting_data.default_osu_img;
    }
    else
    {
        vector<wstring> files;
        wstring file_name;
        //查找文件和歌曲名一致的图片文件
        CFilePathHelper c_file_path(song.file_path);
        //file_name = m_path + c_file_name.GetFileNameWithoutExtension() + L".*";
        c_file_path.ReplaceFileExtension(L"*");
        wstring dir{ c_file_path.GetDir() };
        CCommon::GetImageFiles(c_file_path.GetFilePath(), files);
        if (files.empty() && !song.album.empty())
        {
            //没有找到和歌曲名一致的图片文件，则查找文件名为“唱片集”的文件
            wstring album_name{ song.album };
            CCommon::FileNameNormalize(album_name);
            file_name = dir + album_name + L".*";
            CCommon::GetImageFiles(file_name, files);
        }
        if (!files.empty())
            album_cover_path = dir + files[0];
        // 没有找到唱片集为文件名的文件，查找文件名为设置的专辑封面名的文件
        else if (theApp.m_app_setting_data.use_out_image)
        {
            wstring absolute_dir;
            // 按照默认封面文件名列表查找歌曲所在目录，如遇到绝对路径则存入absolute_dir
            for (const auto& album_name : theApp.m_app_setting_data.default_album_name)
            {
                if (!album_name.empty())
                {
                    if (absolute_dir.empty() && CCommon::IsPath(album_name) && CCommon::FileExist(album_name))
                    {
                        absolute_dir = album_name;
                        continue;
                    }
                    file_name = CCommon::RelativePathToAbsolutePath(album_name + L".*", dir);
                    CCommon::GetImageFiles(file_name, files);
                }
                if (!files.empty())
                {
                    // 处理album_name可能含有相对路径的情况，files[0]仅有文件名
                    // 由于album_name中文件名部分可能含有通配符所以不能只替换后缀，需要替换整个文件名
                    size_t index = file_name.rfind('\\');
                    album_cover_path = file_name.substr(0, index + 1) + files[0];
                    break;
                }
            }
            // 按照歌曲名、唱片名查找封面文件夹album_path
            if (album_cover_path.empty())
            {
                CCommon::GetImageFiles(theApp.m_app_setting_data.AbsoluteAlbumCoverPath() + c_file_path.GetFileName(), files);
                if (files.empty() && !song.album.empty())
                {
                    // 没有找到和歌曲名一致的图片文件，则查找文件名为“唱片集”的文件
                    wstring album_name{ song.album };
                    CCommon::FileNameNormalize(album_name);
                    file_name = theApp.m_app_setting_data.AbsoluteAlbumCoverPath() + album_name + L".*";
                    CCommon::GetImageFiles(file_name, files);
                }
                if (!files.empty())
                    album_cover_path = theApp.m_app_setting_data.AbsoluteAlbumCoverPath() + files[0];
            }
            // 使用默认封面文件名列表中的绝对路径
            if (album_cover_path.empty())
                album_cover_path = absolute_dir;
        }
    }
    return album_cover_path;
}

bool CMusicPlayerCmdHelper::OnRating(const SongInfo& song, DWORD command)
{
    if ((command >= ID_RATING_1 && command <= ID_RATING_5) || command == ID_RATING_NONE)     //如果命令是歌曲分级（应确保分级命令的ID是连续的）
    {
        int rating = 0;
        if (command >= ID_RATING_1 && command <= ID_RATING_5)
            rating = command - ID_RATING_1 + 1;
        SongInfo song_info{ CSongDataManager::GetInstance().GetSongInfo3(song) };
        song_info.rating = static_cast<BYTE>(rating);
        bool succeed{};
        // cue、osu!、不支持写入的文件分级只保存到媒体库
        if (CAudioTag::IsFileRatingSupport(CFilePathHelper(song_info.file_path).GetFileExtension()) && !song_info.is_cue && !COSUPlayerHelper::IsOsuFile(song_info.file_path))
        {
            CAudioTag audio_tag(song_info);
            succeed = audio_tag.WriteAudioRating();
        }
        else
        {
            succeed = true;     //如果文件格式不支持写入分级，也返回true
        }
        CSongDataManager::GetInstance().AddItem(song_info);
        return succeed;
    }
    return true;
}

int CMusicPlayerCmdHelper::UpdateMediaLib()
{
    if (CPlayer::GetInstance().IsMciCore())
        return 0;

    vector<SongInfo> all_media_songs;
    //获取所有音频文件的路径
    for (const auto& item : theApp.m_media_lib_setting_data.media_folders)
    {
        CAudioCommon::GetAudioFiles(item, all_media_songs, MAX_SONG_NUM, true);
    }

    CAudioCommon::GetAudioInfo(all_media_songs,
        theApp.m_media_update_para.num_added,
        theApp.m_media_update_para.thread_exit,
        theApp.m_media_update_para.process_percent,
        theApp.m_media_update_para.force ? MR_FOECE_FULL : MR_FILE_MODIFICATION,
        theApp.m_media_lib_setting_data.ignore_too_short_when_update
    );

    return theApp.m_media_update_para.num_added;
}

int CMusicPlayerCmdHelper::CleanUpSongData(std::function<bool(const SongInfo&)> fun_condition)
{
    return CSongDataManager::GetInstance().RemoveItemIf(fun_condition);
}

int CMusicPlayerCmdHelper::CleanUpRecentFolders()
{
    int cleard_cnt = CRecentList::Instance().RemoveItemIf([](const ListItem& list_item)
        { return list_item.type == LT_FOLDER && !CAudioCommon::IsPathContainsAudioFile(list_item.path, list_item.contain_sub_folder); });
    CRecentList::Instance().SaveData();
    return cleard_cnt;
}

std::wstring CMusicPlayerCmdHelper::GetMediaLibTabName(eMediaLibTab tab)
{
    switch (tab)
    {
    case CMusicPlayerCmdHelper::ML_FOLDER: return theApp.m_str_table.LoadText(L"TXT_FOLDER");
    case CMusicPlayerCmdHelper::ML_PLAYLIST: return theApp.m_str_table.LoadText(L"TXT_PLAYLIST");
    case CMusicPlayerCmdHelper::ML_ARTIST: return theApp.m_str_table.LoadText(L"TXT_ARTIST");
    case CMusicPlayerCmdHelper::ML_ALBUM: return theApp.m_str_table.LoadText(L"TXT_ALBUM");
    case CMusicPlayerCmdHelper::ML_GENRE: return theApp.m_str_table.LoadText(L"TXT_GENRE");
    case CMusicPlayerCmdHelper::ML_YEAR: return theApp.m_str_table.LoadText(L"TXT_YEAR");
    case CMusicPlayerCmdHelper::ML_FILE_TYPE: return theApp.m_str_table.LoadText(L"TXT_FILE_TYPE");
    case CMusicPlayerCmdHelper::ML_BITRATE: return theApp.m_str_table.LoadText(L"TXT_BITRATE");
    case CMusicPlayerCmdHelper::ML_RATING: return theApp.m_str_table.LoadText(L"TXT_RATING");
    }
    return std::wstring();
}

void CMusicPlayerCmdHelper::ShowMediaLib(int cur_tab /*= -1*/, int tab_force_show)
{
    CMusicPlayerDlg* pPlayerDlg = CMusicPlayerDlg::GetInstance();
    if (pPlayerDlg == nullptr)
        return;

    bool dlg_exist = (pPlayerDlg->m_pMediaLibDlg != nullptr && IsWindow(pPlayerDlg->m_pMediaLibDlg->m_hWnd));   //媒体库对话框是否已经存在
    bool tab_not_show = (~theApp.m_media_lib_setting_data.display_item & tab_force_show);       //如果有要强制显示的标签但是媒体库设置中此标签不显示

    //判断要显示的标签是否被隐藏
    bool is_cur_tab_hide{ false };
    if (cur_tab != ML_FOLDER && cur_tab != ML_PLAYLIST)     //文件夹和播放列表标签无法被隐藏
    {
        int test_item_bit = 1 << (cur_tab - 2);
        is_cur_tab_hide = !(theApp.m_media_lib_setting_data.display_item & test_item_bit);
    }

    //没有没有指定要强制显示的标签，且要显示的标签被隐藏，则将cur_tab置为-1，保持上次的标签
    if (tab_force_show == 0 && is_cur_tab_hide)
    {
        cur_tab = -1;
    }
    else
    {
        //计算实际的tab序号（参数cur_tab为媒体库中标签的序号，但是如果有标签不显示，则此序号将不正确，因此这里需要根据实际显示的标签计算出真正的tab序号）
        int shown_tab = theApp.m_media_lib_setting_data.display_item | tab_force_show;  //实际要显示的标签
        int tab_num = cur_tab - 2;             //除去“文件夹”和“播放列表”两个标签（因为这两个标签总是显示，无法隐藏）
        for (int i = 0; i < tab_num; i++)      //遍历cur_tab及前面的所有标签，检查是否有未显示出来的标签
        {
            int tab_mask = 1 << i;
            if ((shown_tab & tab_mask) == 0)  //如果有未显示出来的标签，则当前标签序号减1
                cur_tab--;
        }
    }

    if (dlg_exist && !tab_not_show)
    {
        pPlayerDlg->m_pMediaLibDlg->SetTabForceShow(tab_force_show);
        pPlayerDlg->m_pMediaLibDlg->ShowWindow(SW_SHOWNORMAL);
        pPlayerDlg->m_pMediaLibDlg->SetForegroundWindow();
        pPlayerDlg->m_pMediaLibDlg->SetCurTab(cur_tab);
    }
    else    //如果对话框不存在，或有需要强制显示但是媒体库库中设置不显示的的标签，则需要重新打开媒体库对话框
    {
        CCommon::DeleteModelessDialog(pPlayerDlg->m_pMediaLibDlg);
        pPlayerDlg->m_pMediaLibDlg = new CMediaLibDlg(cur_tab);
        pPlayerDlg->m_pMediaLibDlg->SetTabForceShow(tab_force_show);
        pPlayerDlg->m_pMediaLibDlg->Create(IDD_MEDIA_LIB_DIALOG/*, GetDesktopWindow()*/);
        pPlayerDlg->m_pMediaLibDlg->ShowWindow(SW_SHOW);
    }
}

void CMusicPlayerCmdHelper::RefreshMediaTabData(eMediaLibTab tab_index)
{
    CMusicPlayerDlg* pPlayerDlg = CMusicPlayerDlg::GetInstance();
    if (pPlayerDlg != nullptr && pPlayerDlg->m_pMediaLibDlg != nullptr && IsWindow(pPlayerDlg->m_pMediaLibDlg->GetSafeHwnd()))
    {
        if (tab_index == ML_FOLDER)
            pPlayerDlg->m_pMediaLibDlg->m_path_dlg->RefreshTabData();         // 刷新媒体库文件夹列表
        else if(tab_index == ML_PLAYLIST)
            pPlayerDlg->m_pMediaLibDlg->m_playlist_dlg->RefreshTabData();    // 刷新媒体库播放列表列表
    }
}

void CMusicPlayerCmdHelper::OnViewInMediaLib(eMediaLibTab tab, const std::wstring name)
{
    int tab_force_show{};
    switch (tab)
    {
    case CMusicPlayerCmdHelper::ML_ARTIST: tab_force_show = MLDI_ARTIST; break;
    case CMusicPlayerCmdHelper::ML_ALBUM: tab_force_show = MLDI_ALBUM; break;
    case CMusicPlayerCmdHelper::ML_GENRE: tab_force_show = MLDI_GENRE; break;
    case CMusicPlayerCmdHelper::ML_YEAR: tab_force_show = MLDI_YEAR; break;
    case CMusicPlayerCmdHelper::ML_FILE_TYPE: tab_force_show = MLDI_TYPE; break;
    case CMusicPlayerCmdHelper::ML_BITRATE: tab_force_show = MLDI_BITRATE; break;
    case CMusicPlayerCmdHelper::ML_RATING: tab_force_show = MLDI_RATING; break;
    case CMusicPlayerCmdHelper::ML_ALL: tab_force_show = MLDI_ALL; break;
    }
    ShowMediaLib(tab, tab_force_show);
    CMusicPlayerDlg* pPlayerDlg = CMusicPlayerDlg::GetInstance();
    if (!name.empty())
    {
        if (!pPlayerDlg->m_pMediaLibDlg->NavigateToItem(name))
        {
            wstring type_name = GetMediaLibTabName(tab);
            wstring info = theApp.m_str_table.LoadTextFormat(L"MSG_CANNOT_FIND_IN_MEDIA_LIB_WARNING", { type_name, name });
            pPlayerDlg->MessageBox(info.c_str(), NULL, MB_OK | MB_ICONWARNING);
        }
    }
}

void CMusicPlayerCmdHelper::OnViewInMediaLib(const ListItem& list_item)
{
    CMusicPlayerCmdHelper::eMediaLibTab tab{};
    switch (list_item.type)
    {
    case LT_FOLDER: tab = CMusicPlayerCmdHelper::ML_FOLDER; break;
    case LT_PLAYLIST: tab = CMusicPlayerCmdHelper::ML_PLAYLIST; break;
    case LT_MEDIA_LIB:
        switch (list_item.medialib_type)
        {
        case ListItem::ClassificationType::CT_ARTIST: tab = CMusicPlayerCmdHelper::ML_ARTIST; break;
        case ListItem::ClassificationType::CT_ALBUM: tab = CMusicPlayerCmdHelper::ML_ALBUM; break;
        case ListItem::ClassificationType::CT_GENRE: tab = CMusicPlayerCmdHelper::ML_GENRE; break;
        case ListItem::ClassificationType::CT_YEAR: tab = CMusicPlayerCmdHelper::ML_YEAR; break;
        case ListItem::ClassificationType::CT_TYPE: tab = CMusicPlayerCmdHelper::ML_FILE_TYPE; break;
        case ListItem::ClassificationType::CT_BITRATE: tab = CMusicPlayerCmdHelper::ML_BITRATE; break;
        case ListItem::ClassificationType::CT_RATING: tab = CMusicPlayerCmdHelper::ML_RATING; break;
        case ListItem::ClassificationType::CT_NONE: tab = CMusicPlayerCmdHelper::ML_ALL; break;
        }
    }
    OnViewInMediaLib(tab, list_item.path);
}

void CMusicPlayerCmdHelper::OnViewArtist(const SongInfo& song_info)
{
    vector<wstring> artist_list;
    song_info.GetArtistList(artist_list);     // 获取艺术家（可能有多个）
    wstring artist;
    if (artist_list.empty())
    {
        return;
    }
    else if (artist_list.size() == 1)
    {
        artist = artist_list.front();
    }
    else
    {
        //如果有多个艺术家，弹出“选择艺术家”对话框
        CSelectItemDlg dlg(artist_list);
        dlg.SetTitle(theApp.m_str_table.LoadText(L"TITLE_SELECT_ARTIST").c_str());
        dlg.SetDlgIcon(IconMgr::IconType::IT_Artist);
        if (dlg.DoModal() == IDOK)
            artist = dlg.GetSelectedItem();
        else
            return;
    }
    OnViewInMediaLib(CMusicPlayerCmdHelper::ML_ARTIST, artist);
}

void CMusicPlayerCmdHelper::OnViewAlbum(const SongInfo& song_info)
{
    wstring album = song_info.GetAlbum();
    OnViewInMediaLib(CMusicPlayerCmdHelper::ML_ALBUM, album);
}

int CMusicPlayerCmdHelper::FixPlaylistPathError(const std::wstring& path) const
{
    std::unordered_map<std::wstring, std::set<std::wstring>> song_file_name_map;
    CSongDataManager::GetInstance().GetSongData([&](const CSongDataManager::SongDataMap& song_data_map) {
        for (const auto& song_item : song_data_map)
        {
            if (!song_item.second.is_cue)
            {
                std::wstring file_name = song_item.second.GetFileName();
                song_file_name_map[file_name].insert(song_item.second.file_path);
            }
        }
    });

    vector<SongInfo> song_list;
    CPlaylistFile playlist_file;
    playlist_file.LoadFromFile(path);
    playlist_file.MoveToSongList(song_list);    // move后playlist_file对象不再可用
    int fixed_count{};
    for (auto& song : song_list)
    {
        if (!song.is_cue && !CCommon::FileExist(song.file_path))
        {
            if (FixWrongFilePath(song.file_path, song_file_name_map))
                fixed_count++;
        }
    }
    if (fixed_count > 0)
    {
        //保存播放列表到文件
        CPlaylistFile::SavePlaylistToFile(song_list, path);

        //如果处理的是正在播放的播放列表
        if (CPlayer::GetInstance().IsPlaylistMode() && CPlayer::GetInstance().GetPlaylistPath() == path)
        {
            CPlayer::GetInstance().ReloadPlaylist(MR_MIN_REQUIRED);
        }
    }
    return fixed_count;
}

//计算两个字符串右侧匹配的字符数量
static int CalcualteStringRightMatchedCharNum(const std::wstring& str1, const std::wstring& str2)
{
    size_t index1{ str1.size() - 1 };
    size_t index2{ str2.size() - 1 };
    int char_matched{};
    for (; index1 >= 0 && index2 >= 0; index1--, index2--)
    {
        if (str1[index1] == str2[index2])
            char_matched++;
        else
            break;
    }
    return char_matched;
}

bool CMusicPlayerCmdHelper::FixWrongFilePath(wstring& file_path, const std::unordered_map<std::wstring, std::set<std::wstring>>& song_file_name_map) const
{
    std::wstring file_name{ CFilePathHelper(file_path).GetFileName() };
    bool fixed{ false };
    auto iter = song_file_name_map.find(file_name);
    if (iter != song_file_name_map.end())
    {
        if (iter->second.size() == 1)      //媒体库中同名的文件只有一个时，直接修改为该文件的路径
        {
            if (CCommon::FileExist(*iter->second.begin()))
            {
                file_path = *iter->second.begin();
                fixed = true;
            }
        }
        else if (iter->second.size() > 1)   //媒体库中同名的文件有多个时，查找两个路径末尾相同字符数量最多的那项
        {
            std::wstring best_match_path;
            int max_matched_char_mun{};
            for (const auto& path : iter->second)
            {
                if (!CCommon::FileExist(path))
                    continue;
                int cur_matched_char_num = CalcualteStringRightMatchedCharNum(file_path, path);
                if (cur_matched_char_num > max_matched_char_mun)
                {
                    max_matched_char_mun = cur_matched_char_num;
                    best_match_path = path;
                }
            }
            file_path = best_match_path;
            fixed = true;
        }
    }
    return fixed;
}

void CMusicPlayerCmdHelper::OnListItemSelected(const ListItem& list_item, bool play)
{
    if (list_item.empty())
        return;
    if (!CPlayer::GetInstance().SetList(list_item, play))
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_WAIT_AND_RETRY");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
    }
}

bool CMusicPlayerCmdHelper::OnRenamePlaylist(const ListItem& list_item)
{
    if (list_item.empty() || CRecentList::IsSpecPlaylist(list_item))
        return false;

    CInputDlg imput_dlg;
    imput_dlg.SetTitle(theApp.m_str_table.LoadText(L"TITLE_RENAME_PLAYLIST").c_str());
    imput_dlg.SetInfoText(theApp.m_str_table.LoadText(L"TXT_RENAME_PLAYLIST_INPUT_PLAYLIST_NAME").c_str());
    imput_dlg.SetEditText(CFilePathHelper(list_item.path).GetFileNameWithoutExtension().c_str());
    if (imput_dlg.DoModal() != IDOK)
        return false;

    wstring new_playlist_name{ imput_dlg.GetEditText() };
    if (new_playlist_name.empty())
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_PLAYLIST_NAME_EMPTY_WARNING");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
        return false;
    }
    if (!CCommon::IsFileNameValid(new_playlist_name))
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_FILE_NAME_INVALID_WARNING");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
        return false;
    }
    if (CCommon::FileExist(theApp.m_playlist_dir + new_playlist_name + PLAYLIST_EXTENSION))
    {
        wstring info = theApp.m_str_table.LoadTextFormat(L"MSG_PLAYLIST_EXIST_WARNING", { new_playlist_name });
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
        return false;
    }

    wstring new_path = CCommon::FileRename(list_item.path, new_playlist_name);   //播放列表后命名后的路径
    if (new_path.empty())
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_PLAYLIST_RENANE_FAILED");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
        return false;
    }
    if (list_item.path == new_path)
        return false;
    // 如果重命名的播放是当前播放的播放列表，就重新设置当前播放列表的路径 (之后应该改成进入列表初始化流程)
    if (CRecentList::Instance().IsCurrentList(list_item))
    {
        CPlayer::GetInstance().SetPlaylistPath(new_path);                       // 更新m_playlist_path变量
        theApp.m_pMainWnd->SendMessage(WM_CUR_PLAYLIST_RENAMED);                // 更新主窗口m_path_edit控件文字
    }
    CRecentList::Instance().RenamePlaylist(list_item, new_path);
    CRecentList::Instance().SaveData();
    return true;
}


wstring CMusicPlayerCmdHelper::OnNewPlaylist(const wstring& copy_from_playlist)
{
    CInputDlg imput_dlg(GetOwner());
    imput_dlg.SetTitle(theApp.m_str_table.LoadText(L"TITLE_NEW_PLAYLIST").c_str());
    imput_dlg.SetInfoText(theApp.m_str_table.LoadText(L"TXT_NEW_PLAYLIST_INPUT_PLAYLIST_NAME").c_str());
    if (imput_dlg.DoModal() != IDOK)
        return wstring();
    wstring playlist_name = imput_dlg.GetEditText().GetString();
    if (playlist_name.empty())
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_PLAYLIST_NAME_EMPTY_WARNING");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
        return wstring();
    }
    if (!CCommon::IsFileNameValid(playlist_name))
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_FILE_NAME_INVALID_WARNING");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
        return wstring();
    }
    wstring playlist_path = theApp.m_playlist_dir + playlist_name + PLAYLIST_EXTENSION;
    if (CCommon::FileExist(playlist_path))
    {
        wstring info = theApp.m_str_table.LoadTextFormat(L"MSG_PLAYLIST_EXIST_WARNING", { playlist_name });
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONWARNING | MB_OK);
        return wstring();
    }
    if (copy_from_playlist.empty())
        CRecentList::Instance().AddNewItem(ListItem{ LT_PLAYLIST, playlist_path });
    else
    {
        ListItem list_item{ LT_PLAYLIST, copy_from_playlist };
        if (!CRecentList::Instance().LoadItem(list_item))   // 继承“copy_from_playlist”的 总计时长/总计曲目数
            return wstring();                               // LoadItem返回false说明copy_from_playlist不存在，不继续执行
        CopyFile(copy_from_playlist.c_str(), playlist_path.c_str(), FALSE);
        list_item.path = playlist_path;
        CRecentList::Instance().AddNewItem(list_item);
    }
    CRecentList::Instance().SaveData();
    return playlist_path;

}

void CMusicPlayerCmdHelper::OnPlaylistSaveAs(const std::wstring& playlist_path)
{
    wstring filter = FilterHelper::GetPlaylistSaveAsFilter();
    CFileDialog fileDlg(FALSE, _T("m3u"), CFilePathHelper(playlist_path).GetFileNameWithoutExtension().c_str(), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter.c_str(), GetOwner());
    if (IDOK == fileDlg.DoModal())
    {
        CPlaylistFile playlist;
        playlist.LoadFromFile(playlist_path);
        //将播放列表保存到文件
        wstring file_path{ fileDlg.GetPathName() };
        wstring file_extension{ fileDlg.GetFileExt() };
        file_extension = L'.' + file_extension;
        CPlaylistFile::Type file_type{};
        if (file_extension == PLAYLIST_EXTENSION)
            file_type = CPlaylistFile::PL_PLAYLIST;
        else if (file_extension == L".m3u")
            file_type = CPlaylistFile::PL_M3U;
        else if (file_extension == L".m3u8")
            file_type = CPlaylistFile::PL_M3U8;
        playlist.SaveToFile(file_path, file_type);
    }
}

bool CMusicPlayerCmdHelper::OnPlaylistFixPathError(const std::wstring& playlist_path)
{
    if (!playlist_path.empty())
    {
        const wstring& inquiry_info = theApp.m_str_table.LoadText(L"MSG_PLAYLIST_FIX_ERROR_PATH_INQUIRY");
        if (GetOwner()->MessageBox(inquiry_info.c_str(), NULL, MB_ICONQUESTION | MB_YESNO) == IDYES)
        {
            int fixed_count = FixPlaylistPathError(playlist_path);
            wstring complete_info = theApp.m_str_table.LoadTextFormat(L"MSG_PLAYLIST_FIX_ERROR_PATH_COMPLETE", { fixed_count });
            GetOwner()->MessageBox(complete_info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
            return true;
        }
    }
    return false;
}

bool CMusicPlayerCmdHelper::OnDeleteRecentListItem(const ListItem& list_item)
{
    wstring inquiry_info;
    if (list_item.type == LT_FOLDER)
        inquiry_info = theApp.m_str_table.LoadTextFormat(L"MSG_DELETE_FOLDER_INQUIRY", { list_item.path });
    else if (list_item.type == LT_PLAYLIST && !CRecentList::IsSpecPlaylist(list_item, CRecentList::PT_DEFAULT) && !CRecentList::IsSpecPlaylist(list_item, CRecentList::PT_FAVOURITE))
        inquiry_info = theApp.m_str_table.LoadTextFormat(L"MSG_DELETE_PLAYLIST_INQUIRY", { list_item.GetDisplayName() });
    else
        return false;
    if (GetOwner()->MessageBox(inquiry_info.c_str(), NULL, MB_ICONQUESTION | MB_YESNO) == IDYES)
    {
        // 如果是当前播放则使用CPlayer成员方法处理
        if (CRecentList::Instance().IsCurrentList(list_item))
        {
            if (!CPlayer::GetInstance().RemoveCurPlaylistOrFolder())
            {
                const wstring& info = theApp.m_str_table.LoadText(L"MSG_WAIT_AND_RETRY");
                GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
            }
        }
        else
        {
            if (list_item.type == LT_PLAYLIST)
                CommonDialogMgr::DeleteAFile(GetOwner()->GetSafeHwnd(), list_item.path);
            if (CRecentList::Instance().RemoveItem(list_item))
            {
                CRecentList::Instance().SaveData();
                return true;
            }
        }
    }
    return false;
}

bool CMusicPlayerCmdHelper::OnOpenFolder()
{
    static bool include_sub_dir{ false };
    static CString include_sub_dir_str{ theApp.m_str_table.LoadText(L"TXT_FOLDER_BROWSER_INCLUDE_SUB_DIR").c_str() };
    const wstring& title = theApp.m_str_table.LoadText(L"TITLE_FOLDER_BROWSER_SONG_SOURCE");
#ifdef COMPILE_IN_WIN_XP
    CFolderBrowserDlg folderPickerDlg(this->GetSafeHwnd());
    folderPickerDlg.SetInfo(title.c_str());
#else
    CFilePathHelper current_path(CPlayer::GetInstance().GetCurrentDir());
    CFolderPickerDialog folderPickerDlg(current_path.GetParentDir().c_str());
    folderPickerDlg.m_ofn.lpstrTitle = title.c_str();
    folderPickerDlg.AddCheckButton(IDC_OPEN_CHECKBOX, include_sub_dir_str, include_sub_dir);     //在打开对话框中添加一个复选框
#endif
    if (folderPickerDlg.DoModal() == IDOK)
    {
#ifndef COMPILE_IN_WIN_XP
        BOOL checked;
        folderPickerDlg.GetCheckButtonState(IDC_OPEN_CHECKBOX, checked);
        include_sub_dir = (checked != FALSE);
#endif
        return OnOpenFolder(wstring(folderPickerDlg.GetPathName()), include_sub_dir, false);
    }
    return false;
}

bool CMusicPlayerCmdHelper::OnOpenFolder(std::wstring folder_path, bool include_sub_dir, bool play)
{
    if (!folder_path.empty() && folder_path.back() != L'\\' && folder_path.back() != L'/')
        folder_path.push_back(L'\\');
    ListItem list_item{ LT_FOLDER, folder_path };
    if (CRecentList::Instance().LoadItem(list_item))
    {
        list_item.contain_sub_folder = include_sub_dir;
        OnListItemSelected(list_item, play);
        return true;
    }
    else
    {
        if (!CPlayer::GetInstance().OpenFolder(folder_path, include_sub_dir, play))
        {
            const wstring& info = theApp.m_str_table.LoadText(L"MSG_WAIT_AND_RETRY");
            GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
            return false;
        }
        else
        {
            return true;
        }
    }
}

bool CMusicPlayerCmdHelper::OnRemoveFromPlaylist(const ListItem& list_item, const std::vector<SongInfo>& songs)
{
    if (songs.empty())
        return false;
    ASSERT(list_item.type == LT_PLAYLIST);
    wstring playlist_display_name = list_item.GetDisplayName();
    wstring info;
    if (songs.size() == 1)
    {
        std::wstring song_display_name = CSongInfoHelper::GetDisplayStr(songs.front(), theApp.m_media_lib_setting_data.display_format);
        info = theApp.m_str_table.LoadTextFormat(L"MSG_REMOVE_SINGLE_ITEM_FROM_PLAYLIST_INQUIRY", { playlist_display_name, song_display_name });
    }
    else
    {
        info = theApp.m_str_table.LoadTextFormat(L"MSG_REMOVE_FROM_PLAYLIST_INQUIRY", { playlist_display_name, songs.size() });
    }
    if (GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONQUESTION | MB_YESNO) != IDYES)
        return false;
    //如果当前列表正在播放
    if (CRecentList::Instance().IsCurrentList(list_item))
    {
        std::vector<int> indexs;    // 这里问题很大，之后要重新设计
        for (const auto& song : songs)
        {
            auto iter = std::find(CPlayer::GetInstance().GetPlayList().begin(), CPlayer::GetInstance().GetPlayList().end(), song);
            if (iter != CPlayer::GetInstance().GetPlayList().end())
                indexs.push_back(iter - CPlayer::GetInstance().GetPlayList().begin());
        }
        CPlayer::GetInstance().RemoveSongs(indexs);
    }
    else
    {
        CPlaylistFile playlist_file;
        playlist_file.LoadFromFile(list_item.path);
        for (const auto& song : songs)
        {
            playlist_file.RemoveSong(song);
        }
        playlist_file.SaveToFile(list_item.path);
    }

    //如果是我喜欢的曲目，则需要更新UI中的显示
    if (CRecentList::IsSpecPlaylist(list_item, CRecentList::PT_FAVOURITE))
        CUiMyFavouriteItemMgr::Instance().UpdateMyFavourite();

    return true;
}

bool CMusicPlayerCmdHelper::OnRemoveFromCurrentPlaylist(const std::vector<int>& indexs)
{
    ListItem cur_list = CRecentList::Instance().GetCurrentList();
    if (!indexs.empty() && cur_list.type == LT_PLAYLIST)
    {
        std::wstring playlist_name = cur_list.GetDisplayName();
        std::wstring info;
        if (indexs.size() == 1)
        {
            std::wstring song_display_name;
            int index = indexs.front();
            if (index >= 0 && index < CPlayer::GetInstance().GetSongNum())
            {
                SongInfo song = CPlayer::GetInstance().GetPlayList()[index];
                song_display_name = CSongInfoHelper::GetDisplayStr(song, theApp.m_media_lib_setting_data.display_format);
            }
            info = theApp.m_str_table.LoadTextFormat(L"MSG_REMOVE_SINGLE_ITEM_FROM_PLAYLIST_INQUIRY", { playlist_name, song_display_name });
        }
        else
        {
            info = theApp.m_str_table.LoadTextFormat(L"MSG_REMOVE_FROM_PLAYLIST_INQUIRY", { playlist_name, indexs.size() });
        }
        if (GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONQUESTION | MB_YESNO) == IDYES)
        {
            CPlayer::GetInstance().RemoveSongs(indexs);

            //如果是我喜欢的曲目，则需要更新UI中的显示
            if (CRecentList::Instance().IsCurrentList(CRecentList::Instance().GetSpecPlaylist(CRecentList::PT_FAVOURITE)))
                CUiMyFavouriteItemMgr::Instance().UpdateMyFavourite();

            return true;
        }
    }
    return false;
}

void CMusicPlayerCmdHelper::OnPlayMyFavourite(const SongKey& song_key)
{
    ListItem list_item = CRecentList::Instance().GetSpecPlaylist(CRecentList::PT_FAVOURITE);
    list_item.SetPlayTrack(song_key);
    bool ok = CPlayer::GetInstance().SetList(list_item, true, true);
    if (!ok)
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_WAIT_AND_RETRY");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
    }
}

void CMusicPlayerCmdHelper::OnPlayMyFavourite()
{
    //已经在播放“我喜欢的音乐”
    if (CRecentList::Instance().IsCurrentList(CRecentList::Instance().GetSpecPlaylist(CRecentList::PT_FAVOURITE)))
    {
        //不在播放状态时执行播放命令
        if (!CPlayer::GetInstance().IsPlaying())
            theApp.m_pMainWnd->SendMessage(WM_COMMAND, ID_PLAY);
    }
    //没有播放“我喜欢的音乐”的情况下，播放“我喜欢的音乐”
    else
    {
        OnListItemSelected(CRecentList::Instance().GetSpecPlaylist(CRecentList::PT_FAVOURITE), true);
    }
}

void CMusicPlayerCmdHelper::OnPlayAllTrack(const SongInfo& song)
{
    ListItem list_item{ LT_MEDIA_LIB, L"", ListItem::ClassificationType::CT_NONE };
    list_item.SetPlayTrack(song);
    bool ok = CPlayer::GetInstance().SetList(list_item, true, true);
    if (!ok)
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_WAIT_AND_RETRY");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
    }
}

void CMusicPlayerCmdHelper::OnPlayTrack(int track)
{
    if (!CPlayer::GetInstance().PlayTrack(track))
    {
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_WAIT_AND_RETRY");
        GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
    }
}


bool CMusicPlayerCmdHelper::OnAddRemoveFromFavourite(int track)
{
    if (track < 0 || track >= CPlayer::GetInstance().GetSongNum())
        return false;

    if (CRecentList::Instance().IsPlayingSpecPlaylist(CRecentList::PT_FAVOURITE))
    {
        //如果当前播放列表就是“我喜欢”播放列表，则直接将歌曲从列表中移除
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_REMOVE_FAVOURITE_WARNING");
        if (GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OKCANCEL) == IDOK)
        {
            bool removed = CPlayer::GetInstance().RemoveSong(track);
            if (removed)
            {
                CMusicPlayerDlg* pDlg = dynamic_cast<CMusicPlayerDlg*>(theApp.m_pMainWnd);
                if (pDlg != nullptr)
                    pDlg->ShowPlayList();
                CUiMyFavouriteItemMgr::Instance().UpdateMyFavourite();
            }
            return removed;
        }
    }
    else
    {
        SongInfo song = CPlayer::GetInstance().GetPlayList()[track];
        std::wstring favourite_playlist_path = CRecentList::Instance().GetSpecPlaylist(CRecentList::PT_FAVOURITE).path;
        CPlaylistFile playlist;
        playlist.LoadFromFile(favourite_playlist_path);
        if (!CPlayer::GetInstance().IsFavourite(track))
        {
            //添加到“我喜欢”播放列表
            if (!playlist.IsSongInPlaylist(song))
            {
                playlist.AddSongsToPlaylist(std::vector<SongInfo> {song}, theApp.m_media_lib_setting_data.insert_begin_of_playlist);
                playlist.SaveToFile(favourite_playlist_path);
            }
            CPlayer::GetInstance().SetFavourite(track, true);
        }
        else
        {
            //从“我喜欢”播放列表移除
            playlist.RemoveSong(song);
            playlist.SaveToFile(favourite_playlist_path);
            CPlayer::GetInstance().SetFavourite(track, false);
        }
        CUiMyFavouriteItemMgr::Instance().UpdateMyFavourite();
        return true;
    }
    return false;
}

bool CMusicPlayerCmdHelper::OnAddRemoveFromFavourite(const SongInfo& song)
{
    auto& playlist{ CPlayer::GetInstance().GetPlayList() };
    auto iter = std::find(playlist.begin(), playlist.end(), song);
    if (iter != playlist.end() && CRecentList::Instance().IsPlayingSpecPlaylist(CRecentList::PT_FAVOURITE))
    {
        //如果当前播放列表就是“我喜欢”播放列表，则直接将歌曲从列表中移除
        const wstring& info = theApp.m_str_table.LoadText(L"MSG_REMOVE_FAVOURITE_WARNING");
        if (GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OKCANCEL) == IDOK)
        {
            int track = iter - playlist.begin();
            bool removed = CPlayer::GetInstance().RemoveSong(track);
            if (removed)
            {
                CMusicPlayerDlg* pDlg = dynamic_cast<CMusicPlayerDlg*>(theApp.m_pMainWnd);
                if (pDlg != nullptr)
                    pDlg->ShowPlayList();
                CUiMyFavouriteItemMgr::Instance().UpdateMyFavourite();
            }
            return removed;
        }
    }
    else
    {
        std::wstring favourite_playlist_path = CRecentList::Instance().GetSpecPlaylist(CRecentList::PT_FAVOURITE).path;
        CPlaylistFile favourite_playlist;
        favourite_playlist.LoadFromFile(favourite_playlist_path);
        //添加到“我喜欢”播放列表
        if (!favourite_playlist.IsSongInPlaylist(song))
        {
            favourite_playlist.AddSongsToPlaylist(std::vector<SongInfo> {song}, theApp.m_media_lib_setting_data.insert_begin_of_playlist);
            favourite_playlist.SaveToFile(favourite_playlist_path);

            //如果正在播放“我喜欢的音乐”
            if (CRecentList::Instance().IsPlayingSpecPlaylist(CRecentList::PT_FAVOURITE))
            {
                CPlayer::GetInstance().AddSongsToPlaylist(std::vector<SongInfo> {song});
            }
        }
        else
        {
            //从“我喜欢”播放列表移除
            favourite_playlist.RemoveSong(song);
            favourite_playlist.SaveToFile(favourite_playlist_path);
        }
        CUiMyFavouriteItemMgr::Instance().UpdateMyFavourite();
        return true;

    }

    return false;
}

bool CMusicPlayerCmdHelper::OnAddToFavourite()
{
    if (CRecentList::Instance().IsPlayingSpecPlaylist(CRecentList::PT_FAVOURITE))
        return false;

    SongInfo song = CPlayer::GetInstance().GetCurrentSongInfo();
    std::wstring favourite_playlist_path = CRecentList::Instance().GetSpecPlaylist(CRecentList::PT_FAVOURITE).path;
    CPlaylistFile playlist;
    playlist.LoadFromFile(favourite_playlist_path);
    if (!CPlayer::GetInstance().IsFavourite())
    {
        //添加到“我喜欢”播放列表
        if (!playlist.IsSongInPlaylist(song))
        {
            playlist.AddSongsToPlaylist(std::vector<SongInfo> {song}, theApp.m_media_lib_setting_data.insert_begin_of_playlist);
            playlist.SaveToFile(favourite_playlist_path);
        }
        CPlayer::GetInstance().SetFavourite(true);
        CUiMyFavouriteItemMgr::Instance().UpdateMyFavourite();
        return true;
    }

    return false;
}

void CMusicPlayerCmdHelper::InitSongMultiVersionMenu(const SongInfo& song)
{
    vector<MenuMgr::MenuItem> menu_list;
    const auto& songs_multi_version{ CSongMultiVersionManager::PlaylistMultiVersionSongs().GetSongsMultiVersion(song) };
    if (!songs_multi_version.empty())
    {
        for (const auto& multi_version_song : songs_multi_version)
        {
            UINT id = ID_SONGS_MULTI_VERSION_ITEM_START + menu_list.size();
            if (id <= ID_SONGS_MULTI_VERSION_ITEM_MAX)
            {
                std::wstringstream wss;
                wss << multi_version_song.file_path;
                if (multi_version_song.is_cue)
                {
                    wss << L',' << multi_version_song.track;
                }
                wss << L'\t' << multi_version_song.bitrate << L"kbps";
                menu_list.emplace_back(MenuMgr::MenuItem{ id, IconMgr::IT_NO_ICON, wss.str() });
            }
        }
    }
    else
    {
        std::wstring str = theApp.m_str_table.LoadText(L"TXT_NO_MULTI_VERSION");
        menu_list.emplace_back(MenuMgr::MenuItem{ ID_SONGS_MULTI_VERSION_ITEM_START, IconMgr::IT_NO_ICON, str });
    }
    theApp.m_menu_mgr.UpdateMenu(MenuMgr::SongMultiVersionMenu, menu_list);
    if (songs_multi_version.empty())
        theApp.m_menu_mgr.GetMenu(MenuMgr::SongMultiVersionMenu)->EnableMenuItem(ID_SONGS_MULTI_VERSION_ITEM_START, MF_BYCOMMAND | MF_GRAYED);

}

void CMusicPlayerCmdHelper::OnSetSongMultiVersion(SongInfo& song, int version_index)
{
    if (CSongMultiVersionManager::PlaylistMultiVersionSongs().IsEmpty())
        return;

    const auto& multi_versions{ CSongMultiVersionManager::PlaylistMultiVersionSongs().GetSongsMultiVersion(song) };
    if (version_index >= 0 && version_index < static_cast<int>(multi_versions.size()))
    {
        if (song != multi_versions[version_index])
        {
            bool need_reopen{ song == CPlayer::GetInstance().GetCurrentSongInfo() };
            CPlayer::ReOpen reopen(need_reopen);
            if (reopen.IsLockSuccess())
            {
                CSongMultiVersionManager::PlaylistMultiVersionSongs().SelectSongsMultiVersion(version_index, song);
            }
            else
            {
                const wstring& info = theApp.m_str_table.LoadText(L"MSG_WAIT_AND_RETRY");
                GetOwner()->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
            }
        }
    }
}

void CMusicPlayerCmdHelper::AddToPlaylist(const std::vector<SongInfo>& songs, const std::wstring& playlist_path)
{
    CMusicPlayerDlg* pPlayerDlg = CMusicPlayerDlg::GetInstance();
    if (CPlayer::GetInstance().IsPlaylistMode() && playlist_path == CPlayer::GetInstance().GetPlaylistPath())
    {
        int rtn = CPlayer::GetInstance().AddSongsToPlaylist(songs);
        if (rtn == 0)
        {
            const wstring& info = theApp.m_str_table.LoadText(L"MSG_FILE_EXIST_IN_PLAYLIST");
            pPlayerDlg->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
        }
        else if (rtn == -1)
        {
            const wstring& info = theApp.m_str_table.LoadText(L"MSG_WAIT_AND_RETRY");
            pPlayerDlg->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
        }
        else
        {
            //显示添加成功提示
            CPlayerUIBase* ui = pPlayerDlg->GetCurrentUi();
            if (ui != nullptr)
            {
                wstring playlist_display_name = ListItem(LT_PLAYLIST, playlist_path).GetDisplayName();
                const wstring& info = theApp.m_str_table.LoadTextFormat(L"MSG_ADD_TO_PLAYLIST_SUCCEED", { rtn, playlist_display_name });
                ui->ShowUiTipInfo(info);
            }
        }
    }
    else
    {
        CPlaylistFile playlist;
        playlist.LoadFromFile(playlist_path);
        int rtn = playlist.AddSongsToPlaylist(songs, theApp.m_media_lib_setting_data.insert_begin_of_playlist);
        if (rtn)
        {
            playlist.SaveToFile(playlist_path);
            //显示添加成功提示
            CPlayerUIBase* ui = pPlayerDlg->GetCurrentUi();
            if (ui != nullptr)
            {
                wstring playlist_display_name = ListItem(LT_PLAYLIST, playlist_path).GetDisplayName();
                const wstring& info = theApp.m_str_table.LoadTextFormat(L"MSG_ADD_TO_PLAYLIST_SUCCEED", { rtn, playlist_display_name });
                ui->ShowUiTipInfo(info);
            }
        }
        else
        {
            const wstring& info = theApp.m_str_table.LoadText(L"MSG_FILE_EXIST_IN_PLAYLIST");
            pPlayerDlg->MessageBox(info.c_str(), NULL, MB_ICONINFORMATION | MB_OK);
        }
    }
}

CWnd* CMusicPlayerCmdHelper::GetOwner()
{
    if (m_pOwner != nullptr)
    {
        return m_pOwner;
    }
    else
    {
        CMusicPlayerDlg* dlg = CMusicPlayerDlg::GetInstance();
        if (dlg != nullptr && dlg->IsMiniMode())
            return dlg->GetMinimodeDlg();
        return theApp.m_pMainWnd;
    }
}
