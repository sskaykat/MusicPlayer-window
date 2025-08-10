#include "stdafx.h"
#include "SongMultiVersion.h"
#include "SongDataManager.h"

void CSongMultiVersion::MergeSongsMultiVersion(std::vector<SongInfo>& songs)
{
	m_duplicate_songs.clear();
	std::map<std::wstring, SongInfo> song_map;
	for (auto iter = songs.begin(); iter != songs.end();)
	{
		const auto& song{ *iter };
		std::wstring key{ MakeKey(song) };
		auto iter2 = song_map.find(key);
		//���ظ���
		if (iter2 != song_map.end())
		{
			if (m_duplicate_songs[key].empty())
				m_duplicate_songs[key].push_back(iter2->second);
			m_duplicate_songs[key].push_back(song);

			//���ظ�����б����Ƴ�
			iter = songs.erase(iter);
		}
		else
		{
			++iter;
		}
		song_map[key] = song;
	}

	//���б����ж���汾��������Ϊѡ�еİ汾
	for (auto& song : songs)
	{
		auto& song_multi_version{ GetSongsMultiVersion(song) };
		if (!song_multi_version.empty())
		{
			bool has_prefered = false;
			for (int i = 0; i < static_cast<int>(song_multi_version.size()); i++)
			{
				const SongInfo& selected_song = song_multi_version[i];
				if (IsSongPrefered(selected_song))
				{
					song = selected_song;
					has_prefered = true;
				}
			}
			//�����û��ѡ���һ���汾����Ĭ��ѡ���������ߵ���
			int prefered_index = 0;
			int max_bitrate = 0;
			for (int i = 0; i < static_cast<int>(song_multi_version.size()); i++)
			{
				const SongInfo& selected_song = song_multi_version[i];
				if (selected_song.bitrate > max_bitrate)
				{
					max_bitrate = selected_song.bitrate;
					prefered_index = i;
				}
			}
			if (prefered_index > 0)
				song = song_multi_version[prefered_index];
		}
	}
}

void CSongMultiVersion::SelectSongsMultiVersion(int index, SongInfo& cur_song)
{
	auto iter = m_duplicate_songs.find(MakeKey(cur_song));
	if (iter != m_duplicate_songs.end())
	{
		//��ȡ֮ǰѡ��İ汾���
		int selected_index = -1;
		for (int i = 0; i < static_cast<int>(iter->second.size()); i++)
		{
			if (IsSongPrefered(iter->second[i]))
			{
				selected_index = i;
				break;
			}
		}
		//����ǰ��Ŀ���浽֮ǰѡ�еİ汾
		if (selected_index >= 0)
			iter->second[selected_index] = cur_song;

		//�����µ�ѡ�а汾
		for (int i = 0; i < static_cast<int>(iter->second.size()); i++)
		{
			bool is_prefered = (i == index);
			const SongInfo& selected_song = iter->second[i];
			SetSongPrefered(selected_song, is_prefered);
			if (is_prefered)
				cur_song = selected_song;
		}
	}
}

int CSongMultiVersion::GetSongMultiVersionIndex(const SongInfo& cur_song)
{
	auto iter = m_duplicate_songs.find(MakeKey(cur_song));
	if (iter != m_duplicate_songs.end())
	{
		for (int i = 0; i < static_cast<int>(iter->second.size()); i++)
		{
			if (IsSongPrefered(iter->second[i]))
				return i;
		}
	}
	return 0;
}

const std::vector<SongInfo>& CSongMultiVersion::GetSongsMultiVersion(const SongInfo& song)
{
	const static std::vector<SongInfo> empty_vec;
	if (m_duplicate_songs.empty())
		return empty_vec;
	auto iter = m_duplicate_songs.find(MakeKey(song));
	if (iter != m_duplicate_songs.end())
	{
		return iter->second;
	}
	return empty_vec;
}

bool CSongMultiVersion::IsEmpty() const
{
	return m_duplicate_songs.empty();
}

std::wstring CSongMultiVersion::MakeKey(const SongInfo& song_info)
{
	//�������⡢�����Ҷ���Ϊ��ʱ�������⡢�����ҡ���Ƭ����ͬ����Ŀ��Ϊͬһ����Ŀ�Ĳ�ͬ�汾
	std::wstringstream wss;
	if (!song_info.title.empty() && !song_info.artist.empty())
	{
		wss << song_info.title << L'|';
		std::vector<std::wstring> artist_list;
		song_info.GetArtistList(artist_list);
		for (const auto& artist : artist_list)
			wss << artist << L';';
		wss << L'|' << song_info.album;
		return wss.str();
	}
	else
	{
		if (song_info.is_cue)
			return song_info.file_path + L'|' + std::to_wstring(song_info.track);
		else
			return song_info.file_path;
	}
}

bool CSongMultiVersion::IsSongPrefered(const SongInfo& song_info)
{
	SongInfo song = CSongDataManager::GetInstance().GetSongInfo3(song_info);
	return song.is_prefered;
}

void CSongMultiVersion::SetSongPrefered(const SongInfo& song_info, bool is_prefered)
{
	SongInfo song = CSongDataManager::GetInstance().GetSongInfo3(song_info);
	song.is_prefered = is_prefered;
	CSongDataManager::GetInstance().AddItem(song);
}

void CSongMultiVersion::Clear()
{
	m_duplicate_songs.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
CSongMultiVersion CSongMultiVersionManager::m_playlist_multi_version_songs;

CSongMultiVersion& CSongMultiVersionManager::PlaylistMultiVersionSongs()
{
	return m_playlist_multi_version_songs;
}
