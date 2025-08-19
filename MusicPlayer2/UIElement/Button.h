#pragma once
#include "UIElement/UIElement.h"
namespace UiElement
{
    //��ť
    class Button : public Element
    {
    public:
        CPlayerUIBase::BtnKey key;      //��ť������
        bool big_icon{};                //���Ϊfalse����ͼ��ߴ�Ϊ16x16������Ϊ20x20
        bool show_text{};               //�Ƿ���ͼ���Ҳ���ʾ�ı�
        int font_size{ 9 };             //�����С������show_textΪtrueʱ��Ч
        virtual void Draw() override;
        void FromString(const std::string& key_type);
        virtual int GetMaxWidth(CRect parent_rect) const override;
        virtual void ClearRect() override;
    };
}

