#pragma once
#include "SongInfo.h"

//���ڹ���ǰ�����б���ͬһ�׸����Ĳ�ͬ�汾
class CSongMultiVersion
{
public:
    /**
     * @brief   ����һ����Ŀ�б���ͬһ����Ŀ�Ĳ�ͬ�汾���������Ǻϲ����������ı��⡢�����ҡ���Ƭ����ͬ����Ϊ��ͬһ����Ŀ�Ĳ�ͬ�汾��
     * @param[in]   songs Ҫ�ϲ�����Ŀ�б�
     */
    void MergeSongsMultiVersion(std::vector<SongInfo>& songs);

    /**
     * @brief   �����ڲ��Ŷ��е�һ����Ŀ�ж���汾ʱ��ѡ����Ŀ��һ���汾
     * @param   index ��Ŀ�汾���
     * @param   cur_song ���ڲ��Ŷ����е���Ŀ�������滻Ϊѡ�еİ汾��
     */
    void SelectSongsMultiVersion(int index, SongInfo& cur_song);

    /**
     * @brief   �����ڲ��Ŷ��е�һ����Ŀ�ж���汾ʱ����ȡ��ǰѡ�еİ汾
     * @param   cur_song ���ڲ��Ŷ����е���Ŀ
     * @return 
     */
    int GetSongMultiVersionIndex(const SongInfo& cur_song);

    /**
     * @brief   ��ȡ���Ŷ��е�һ����Ŀ�Ķ���汾
     * @param   song ���ڲ��Ŷ����е���Ŀ
     * @return 
     */
    const std::vector<SongInfo>& GetSongsMultiVersion(const SongInfo& song);

    /**
     * @brief   �Ƿ�Ϊ�գ����ڲ��Ŷ�����û����Ŀ�ж���汾��
     * @return 
     */
    bool IsEmpty() const;

    //�����Ŀ�Ķ���汾
    void Clear();

protected:
    std::wstring MakeKey(const SongInfo& song_info);

    //��SongDataMap�в�ѯ�Ƿ�Ϊѡ�е���Ŀ
    bool IsSongPrefered(const SongInfo& song_info);

    //���Ƿ�ѡ�б�־д�뵽SongDataMap
    void SetSongPrefered(const SongInfo& song_info, bool is_prefered);

private:
    std::map<std::wstring, std::vector<SongInfo>> m_duplicate_songs;    //����ͬһ��Ŀ�Ĳ�ͬ�汾��key Ϊ SongInfo �е� "title|artist|album"��
};

///////////////////////////////////////////////////////////////////////////////////////////
class CSongMultiVersionManager
{
public:
    static CSongMultiVersion& PlaylistMultiVersionSongs();

private:
    static CSongMultiVersion m_playlist_multi_version_songs;
};
