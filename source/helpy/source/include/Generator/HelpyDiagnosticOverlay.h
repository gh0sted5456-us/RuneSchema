#pragma once
#include "Core/DiagnosticToastPresentation.h"
#include "Core/HelpyOverlayGeometry.h"
#include "Core/RsmkResources.h"
#include "Core/LoadingIndicator.h"
#include "HelpyHudLayout.h"
#include "Generator/HelpyImGuiTheme.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#pragma comment(lib,"msimg32.lib")

namespace PS {
// An input-transparent Win32 surface, independent of the hidden F2 swapchain.
// Paint is simple, transient GDI work: no engine objects, window activation,
// Direct3D device recreation, or additional threads.
class HelpyDiagnosticOverlay {
public:
    enum class Mode{Notifications,Coordinates,Startup};
    struct StartupPng {
        UINT Width{},Height{};
        std::vector<unsigned char> Pixels;
        float Aspect()const{return Height?float(Width)/Height:1.f;}
    };
private:
    static constexpr const wchar_t* ClassName=L"RuneSchema.Helpy.DiagnosticToasts.v1";
    static constexpr COLORREF ColorKey=RGB(255,0,255);
    HINSTANCE m_module{};
    HWND m_window{};
    bool m_registered=false,m_showing=false;
    HWND m_game{};HMONITOR m_monitor{};UINT m_dpi=96;
    std::chrono::steady_clock::time_point m_nextUpdate{};
    std::vector<DiagnosticToastFeed::Notice> m_cards;
    ServerModVersions::Summary m_versions;
    std::string m_fps;
    HelpyImGuiTheme::Theme m_theme{};
    float m_scale=1.0f;
    int m_width=390,m_gap=8;
    HICON m_logo{};bool m_viewMods=false;
    StartupPng m_startupRune,m_startupGame,m_startupWheel;
    Mode m_mode=Mode::Notifications;
    LoadingIndicator::StartupLayout m_startupLayout;
    std::chrono::steady_clock::time_point m_started{};bool m_animate=true;
    struct Layout {int DetailHeight=0,FooterHeight=0,Height=0;};
    std::vector<Layout> m_layouts;

    static COLORREF Color(const ImVec4& value){
        const auto channel=[](float x){return static_cast<int>(std::lround(std::clamp(x,0.0f,1.0f)*255.0f));};
        return RGB(channel(value.x),channel(value.y),channel(value.z));
    }
    static COLORREF Blend(COLORREF a,COLORREF b,float amount){
        const auto mix=[&](int x,int y){return static_cast<int>(std::lround(x+(y-x)*amount));};
        return RGB(mix(GetRValue(a),GetRValue(b)),mix(GetGValue(a),GetGValue(b)),mix(GetBValue(a),GetBValue(b)));
    }
    static std::wstring Wide(std::string_view text){
        if(text.empty())return {};
        int chars=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
        const UINT codepage=chars?CP_UTF8:CP_ACP;
        if(!chars)chars=MultiByteToWideChar(codepage,0,text.data(),static_cast<int>(text.size()),nullptr,0);
        if(!chars)return L"(message unavailable)";
        std::wstring result(static_cast<size_t>(chars),L'\0');
        MultiByteToWideChar(codepage,codepage==CP_UTF8?MB_ERR_INVALID_CHARS:0,text.data(),static_cast<int>(text.size()),result.data(),chars);
        return result;
    }
    int HudHeight()const{return m_mode==Mode::Startup?int(std::ceil(m_startupLayout.Height)):Spacing(m_mode==Mode::Coordinates&&m_fps.find('\n')!=m_fps.npos?56:32);}
    int StackHeight() const {
        int total=m_fps.empty()?0:HudHeight()+(m_layouts.empty()?0:m_gap);
        for(size_t i=0;i<m_layouts.size();++i)total+=m_layouts[i].Height+(i?m_gap:0);
        return total;
    }
    struct Fonts {
        HFONT Small{},Heading{},Regular{};
        int SmallSize=0,TitleSize=0,BodySize=0;
        Fonts(const HelpyImGuiTheme::Theme& theme,float scale){
            SmallSize=std::max(10,static_cast<int>(std::lround(10*scale*theme.fontScale)));
            TitleSize=std::max(13,static_cast<int>(std::lround(14*scale*theme.fontScale)));
            BodySize=std::max(11,static_cast<int>(std::lround(12*scale*theme.fontScale)));
            const auto family=Wide(theme.fontFamily);
            const auto* name=family.empty()?L"Segoe UI":family.c_str();
            Small=CreateFontW(-SmallSize,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,name);
            Heading=CreateFontW(-TitleSize,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,name);
            Regular=CreateFontW(-BodySize,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,name);
        }
        ~Fonts(){if(Small)DeleteObject(Small);if(Heading)DeleteObject(Heading);if(Regular)DeleteObject(Regular);}
        Fonts(const Fonts&)=delete;Fonts& operator=(const Fonts&)=delete;
    };
    int Spacing(float n)const {return std::max(1,static_cast<int>(std::lround(n*m_scale*m_theme.density)));}
    static int WrapHeight(HDC dc,HFONT font,const std::wstring& message,int width){
        if(message.empty())return 0;
        const HGDIOBJ previous=SelectObject(dc,font);
        RECT rect{0,0,std::max(1,width),0};
        DrawTextW(dc,message.c_str(),static_cast<int>(message.size()),&rect,
            DT_WORDBREAK|DT_EDITCONTROL|DT_CALCRECT|DT_NOPREFIX);
        SelectObject(dc,previous);
        return std::max(0,static_cast<int>(rect.bottom-rect.top))+2;
    }
    void MeasureCards(HDC dc){
        m_layouts.clear();m_layouts.reserve(m_cards.size());
        const Fonts fonts(m_theme,m_scale);
        const int stripeWidth=std::max(3,Spacing(4));
        const int textWidth=std::max(60,m_width-2*Spacing(16)-stripeWidth-8);
        for(const auto& card:m_cards){
            const auto display=DiagnosticToastPresentation::Format(card,m_versions);
            Layout item;
            item.DetailHeight=WrapHeight(dc,fonts.Regular,Wide(display.Detail),textWidth);
            item.FooterHeight=display.Subtitle.empty()?0:WrapHeight(dc,fonts.Small,Wide(display.Subtitle),textWidth);
            // Measure the same font and gap geometry that Paint uses. Reserve
            // bottom padding *after* the last wrapped glyph, at every DPI.
            const int header=std::max({fonts.SmallSize+Spacing(6),Spacing(16),std::max(12,Spacing(display.WorldMods?24:15))});
            const int title=fonts.TitleSize+Spacing(6);
            item.Height=Spacing(11)+header+Spacing(5)+title+Spacing(6)+item.DetailHeight
                +(item.FooterHeight?Spacing(5)+item.FooterHeight:0)+Spacing(15);
            m_layouts.push_back(item);
        }
    }
    static void Icon(HDC dc,DiagnosticToastPresentation::Display::Icon type,int x,int y,int diameter,COLORREF tint){
        const int stroke=std::max(1,diameter/9);
        const HPEN pen=CreatePen(PS_SOLID,stroke,tint);
        const HGDIOBJ priorPen=SelectObject(dc,pen),priorBrush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
        const int cx=x+diameter/2,cy=y+diameter/2,p=std::max(3,diameter/5);
        if(type==DiagnosticToastPresentation::Display::Icon::Warning){
            POINT triangle[]={{cx,y+2},{x+diameter-2,y+diameter-2},{x+2,y+diameter-2},{cx,y+2}};
            Polyline(dc,triangle,4);
            MoveToEx(dc,cx,cy-2,nullptr);LineTo(dc,cx,cy+p);
            Ellipse(dc,cx-1,cy+p+2,cx+2,cy+p+5);
        }else if(type==DiagnosticToastPresentation::Display::Icon::Activity){
            for(int row=0;row<3;++row){RECT r{x+2+row,y+2+row,x+diameter-4+row,y+diameter-4+row};
                if(r.left<r.right&&r.top<r.bottom)Rectangle(dc,r.left,r.top,r.right,r.bottom);}
        }else{
            Ellipse(dc,x+1,y+1,x+diameter-1,y+diameter-1);
            if(type==DiagnosticToastPresentation::Display::Icon::Error){
                MoveToEx(dc,cx-p,cy-p,nullptr);LineTo(dc,cx+p,cy+p);
                MoveToEx(dc,cx+p,cy-p,nullptr);LineTo(dc,cx-p,cy+p);
            }else if(type==DiagnosticToastPresentation::Display::Icon::Debug){
                POINT left[]={{cx-p,cy-p},{cx-2*p,cy},{cx-p,cy+p}};
                POINT right[]={{cx+p,cy-p},{cx+2*p,cy},{cx+p,cy+p}};
                Polyline(dc,left,3);Polyline(dc,right,3);
            }else if(type==DiagnosticToastPresentation::Display::Icon::Diagnostic){
                POINT wave[]={{x+3,cy},{cx-p,cy},{cx-2,cy-p},{cx+1,cy+p},{cx+p,cy},{x+diameter-3,cy}};
                Polyline(dc,wave,6);
            }else{
                MoveToEx(dc,cx,cy-1,nullptr);LineTo(dc,cx,cy+p);
                Ellipse(dc,cx-1,cy-p-4,cx+2,cy-p-1);
            }
        }
        SelectObject(dc,priorBrush);SelectObject(dc,priorPen);DeleteObject(pen);
    }
    static void RoundedBox(HDC dc,const RECT& rc,int radius,COLORREF fill,COLORREF border){
        const HBRUSH brush=CreateSolidBrush(fill);
        const HPEN pen=CreatePen(PS_SOLID,1,border);
        const HGDIOBJ priorBrush=SelectObject(dc,brush),priorPen=SelectObject(dc,pen);
        RoundRect(dc,rc.left,rc.top,rc.right,rc.bottom,radius*2,radius*2);
        SelectObject(dc,priorPen);SelectObject(dc,priorBrush);
        DeleteObject(pen);DeleteObject(brush);
    }
    static void Text(HDC dc,HFONT font,COLORREF color,std::wstring_view text,RECT rc,UINT alignment){
        const auto old=SelectObject(dc,font);
        SetTextColor(dc,color);
        DrawTextW(dc,text.data(),static_cast<int>(text.size()),&rc,alignment|DT_NOPREFIX);
        SelectObject(dc,old);
    }
    struct StartupSurface {
        HDC Dc=CreateCompatibleDC(nullptr);
        HBITMAP Bitmap{};
        HGDIOBJ Previous{};
        uint32_t* Pixels{};
        int Width,Height;
        StartupSurface(int width,int height):Width(width),Height(height){
            BITMAPINFO info{};
            info.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB};
            void* bits=nullptr;
            if(Dc)Bitmap=CreateDIBSection(Dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
            if(Bitmap&&bits){
                Previous=SelectObject(Dc,Bitmap);
                Pixels=static_cast<uint32_t*>(bits);
                std::fill_n(Pixels,size_t(width)*height,0u);
            }
        }
        ~StartupSurface(){if(Previous)SelectObject(Dc,Previous);if(Bitmap)DeleteObject(Bitmap);if(Dc)DeleteDC(Dc);}
    };
    static void BlendStartupPng(StartupSurface& dst,const StartupPng& image,float cx,float cy,
                                float width,float height,float opacity,float angle=0.f){
        if(image.Pixels.empty()||opacity<=0)return;
        const float cosine=std::cos(angle),sine=std::sin(angle);
        const float hx=(std::abs(cosine)*width+std::abs(sine)*height)*.5f;
        const float hy=(std::abs(sine)*width+std::abs(cosine)*height)*.5f;
        for(int y=std::max(0,int(std::floor(cy-hy)));y<std::min(dst.Height,int(std::ceil(cy+hy)));++y)
            for(int x=std::max(0,int(std::floor(cx-hx)));x<std::min(dst.Width,int(std::ceil(cx+hx)));++x){
                const float dx=x+.5f-cx,dy=y+.5f-cy;
                const float u=(cosine*dx+sine*dy)/width+.5f,v=(-sine*dx+cosine*dy)/height+.5f;
                if(u<0||u>=1||v<0||v>=1)continue;
                const auto* rgba=&image.Pixels[(size_t(v*image.Height)*image.Width+size_t(u*image.Width))*4];
                const unsigned alpha=unsigned(rgba[3]*opacity+.5f),keep=255-alpha;
                if(!alpha)continue;
                auto& out=dst.Pixels[size_t(y)*dst.Width+x];
                const auto channel=[&](unsigned index,unsigned shift){
                    return (rgba[index]*alpha+((out>>shift)&255)*keep+127)/255;
                };
                out=((alpha+((out>>24)*keep+127)/255)<<24)|(channel(0,16)<<16)|(channel(1,8)<<8)|channel(2,0);
            }
    }
    void PaintStartup(StartupSurface& surface,double elapsed){
        const Fonts fonts(m_theme,m_scale);
        SetBkMode(surface.Dc,TRANSPARENT);
        const auto split=m_fps.find('\n');
        RECT label{Spacing(8),int(m_startupLayout.TextY),m_width-Spacing(8),int(m_startupLayout.TipY)};
        Text(surface.Dc,fonts.Regular,RGB(255,255,255),Wide(m_fps.substr(0,split)),label,DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        if(split!=m_fps.npos){
            RECT tip{Spacing(12),int(m_startupLayout.TipY),m_width-Spacing(12),surface.Height-Spacing(8)};
            Text(surface.Dc,fonts.Small,RGB(255,255,255),Wide(m_fps.substr(split+1)),tip,DT_CENTER|DT_WORDBREAK|DT_END_ELLIPSIS);
        }
        GdiFlush();
        const auto color=Color(m_theme.text);
        for(int y=std::max(0,int(m_startupLayout.TextY));y<surface.Height;++y)
            for(int x=0;x<surface.Width;++x){
                auto& pixel=surface.Pixels[size_t(y)*surface.Width+x];
                const auto coverage=std::max({pixel&255u,(pixel>>8)&255u,(pixel>>16)&255u});
                pixel=(coverage<<24)|((GetRValue(color)*coverage/255)<<16)|
                    ((GetGValue(color)*coverage/255)<<8)|(GetBValue(color)*coverage/255);
            }
        const auto animation=LoadingIndicator::Startup(elapsed,m_animate);
        const float center=m_width*.5f,logoY=m_startupLayout.LogoY,side=m_startupLayout.RuneSize;
        const float gameWidth=m_startupLayout.GameWidth;
        BlendStartupPng(surface,m_startupGame,center,logoY,gameWidth,
                        gameWidth/std::max(.2f,m_startupGame.Aspect()),1-animation.RuneAlpha);
        BlendStartupPng(surface,m_startupRune,center,logoY,side,side,
                        m_startupGame.Pixels.empty()?1.f:animation.RuneAlpha);
        const float wheel=m_startupLayout.WheelRadius*2;
        BlendStartupPng(surface,m_startupWheel,center,m_startupLayout.WheelY,wheel,wheel,1,animation.Angle);
    }
    void Paint(HDC dc,RECT bounds){
        const HBRUSH clear=CreateSolidBrush(ColorKey);
        FillRect(dc,&bounds,clear);DeleteObject(clear);
        if(m_cards.empty()&&m_fps.empty())return;
        SetBkMode(dc,TRANSPARENT);
        const Fonts fonts(m_theme,m_scale);
        const COLORREF bg=Color(m_theme.panelAlt),baseBorder=Color(m_theme.border),secondary=Color(m_theme.textDisabled),mainText=Color(m_theme.text);
        const int radius=std::clamp(static_cast<int>(std::lround(m_theme.childRounding*m_scale)),5,28);
        const int stripeWidth=std::max(3,Spacing(4));
        int top=0;
        if(!m_fps.empty()){
            const int height=HudHeight();RECT box{0,0,m_width,height};
            const auto brush=CreateSolidBrush(bg);FillRect(dc,&box,brush);DeleteObject(brush);
            if(m_mode==Mode::Coordinates&&m_fps.find('\n')!=m_fps.npos)Text(dc,fonts.Small,mainText,Wide(m_fps),box,DT_CENTER|DT_WORDBREAK);
            else Text(dc,fonts.Heading,mainText,Wide(m_fps),box,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            top=height+m_gap;
        }
        for(size_t n=0;n<m_cards.size()&&n<m_layouts.size();++n){
            const auto& card=m_cards[n];const auto& layout=m_layouts[n];
            const auto formatted=DiagnosticToastPresentation::Format(card,m_versions);
            const int height=layout.Height;
            RECT shadow{4,top+4,m_width-1,top+height-1};
            RoundedBox(dc,shadow,radius,Blend(bg,RGB(0,0,0),0.55f),Blend(baseBorder,RGB(0,0,0),0.65f));
            const auto severity=formatted.Success?ImVec4(.35f,.8f,.48f,1):formatted.WorldMods?(formatted.Alignment==ServerModVersions::Tone::Aligned?ImVec4(.35f,.8f,.48f,1):formatted.Alignment==ServerModVersions::Tone::Differences?m_theme.warning:m_theme.info):card.Summary?m_theme.accent:card.Severity==LiveLog::Level::Error?m_theme.error:card.Severity==LiveLog::Level::Warning?m_theme.warning:card.Severity==LiveLog::Level::Diagnostic?m_theme.accent:m_theme.info;
            const COLORREF tint=Color(severity);
            RECT body{1,top+1,m_width-5,top+height-5};
            RoundedBox(dc,body,radius,bg,(formatted.WorldMods||formatted.Success)?tint:baseBorder);
            const HBRUSH stripe=CreateSolidBrush(tint);
            RECT side{body.left+3,body.top+radius,body.left+3+stripeWidth,body.bottom-radius};
            FillRect(dc,&side,stripe);DeleteObject(stripe);
            const int left=body.left+Spacing(16)+stripeWidth;
            const int right=body.right-Spacing(16);
            const int headerY=top+Spacing(11);
            const int iconSize=std::max(12,Spacing(formatted.WorldMods?24:15));
            if(formatted.WorldMods&&m_logo)DrawIconEx(dc,left,headerY,m_logo,iconSize,iconSize,0,nullptr,DI_NORMAL);
            else Icon(dc,formatted.Type,left,headerY,iconSize,tint);
            const int headerHeight=std::max({fonts.SmallSize+Spacing(6),Spacing(16),iconSize});
            // The icon has its own fixed slot, so the severity badge cannot
            // push a long translated brand/title into the adjacent region.
            Text(dc,fonts.Small,secondary,L"RuneSchema",RECT{left+iconSize+Spacing(7),headerY,right,headerY+headerHeight},DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
            auto label=Wide(card.Summary?"ACTIVITY":formatted.Level);
            if(card.Repetitions>1&&!card.Summary&&!(card.BlueprintBatch&&card.BatchCount>1))label+=L"  x"+std::to_wstring(card.Repetitions);
            SIZE labelSize{};const auto old=SelectObject(dc,fonts.Small);
            GetTextExtentPoint32W(dc,label.c_str(),static_cast<int>(label.size()),&labelSize);SelectObject(dc,old);
            const int badgeWidth=static_cast<int>(labelSize.cx)+Spacing(16);
            RECT pill{right-badgeWidth,headerY-1,right,headerY+headerHeight+1};
            RoundedBox(dc,pill,std::max(4,headerHeight/2),Blend(bg,tint,0.16f),Blend(baseBorder,tint,0.28f));
            Text(dc,fonts.Small,tint,label,RECT{pill.left+4,pill.top+1,pill.right-4,pill.bottom},DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
            const int titleY=headerY+headerHeight+Spacing(5);
            Text(dc,fonts.Heading,mainText,Wide(formatted.Title),RECT{left,titleY,right,titleY+fonts.TitleSize+Spacing(6)},DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
            const int detailY=titleY+fonts.TitleSize+Spacing(6)+Spacing(6);
            const auto detail=Wide(formatted.Detail);
            Text(dc,fonts.Regular,secondary,detail,RECT{left,detailY,right,detailY+layout.DetailHeight},DT_LEFT|DT_WORDBREAK|DT_EDITCONTROL);
            if(layout.FooterHeight){
                const int footerY=detailY+layout.DetailHeight+Spacing(5);
                Text(dc,fonts.Small,secondary,Wide(formatted.Subtitle),RECT{left,footerY,right,footerY+layout.FooterHeight},DT_LEFT|DT_WORDBREAK|DT_EDITCONTROL);
            }
            top+=height+m_gap;
        }
    }
    bool ModsAt(POINT point)const{
        int top=m_fps.empty()?0:Spacing(32)+m_gap;
        for(size_t i=0;i<m_cards.size()&&i<m_layouts.size();++i){const int bottom=top+m_layouts[i].Height;
            if(point.x>=0&&point.x<m_width&&point.y>=top&&point.y<bottom)return DiagnosticToastPresentation::Format(m_cards[i]).WorldMods;
            top=bottom+m_gap;
        }return false;
    }
    static LRESULT CALLBACK WindowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam){
        auto* self=reinterpret_cast<HelpyDiagnosticOverlay*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){auto* create=reinterpret_cast<CREATESTRUCTW*>(lParam);self=static_cast<HelpyDiagnosticOverlay*>(create->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(message==WM_NCHITTEST){POINT point{static_cast<short>(LOWORD(lParam)),static_cast<short>(HIWORD(lParam))};ScreenToClient(hwnd,&point);return self&&self->ModsAt(point)?HTCLIENT:HTTRANSPARENT;}
        if(message==WM_LBUTTONUP&&self){POINT point{static_cast<short>(LOWORD(lParam)),static_cast<short>(HIWORD(lParam))};if(self->ModsAt(point))self->m_viewMods=true;return 0;}
        if(message==WM_SETCURSOR&&LOWORD(lParam)==HTCLIENT){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}
        if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
        if(message==WM_DPICHANGED&&self){self->m_dpi=HIWORD(wParam);self->m_nextUpdate={};return 0;}
        if(message==WM_SETTINGCHANGE&&self){BOOL animate=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0);self->m_animate=animate!=FALSE;self->m_nextUpdate={};InvalidateRect(hwnd,nullptr,FALSE);return 0;}
        if(message==WM_ERASEBKGND)return 1;
        if(message==WM_PAINT){
            PAINTSTRUCT paint{};const auto dc=BeginPaint(hwnd,&paint);RECT bounds{};GetClientRect(hwnd,&bounds);
            if(self&&self->m_mode==Mode::Startup){EndPaint(hwnd,&paint);return 0;}
            const auto buffer=CreateCompatibleDC(dc);const auto bitmap=CreateCompatibleBitmap(dc,std::max(1L,bounds.right),std::max(1L,bounds.bottom));
            if(buffer&&bitmap){const auto prior=SelectObject(buffer,bitmap);if(self)self->Paint(buffer,bounds);BitBlt(dc,0,0,bounds.right,bounds.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,prior);}
            else if(self)self->Paint(dc,bounds);
            if(bitmap)DeleteObject(bitmap);if(buffer)DeleteDC(buffer);EndPaint(hwnd,&paint);return 0;
        }
        return DefWindowProcW(hwnd,message,wParam,lParam);
    }
    bool Ensure(HINSTANCE module){
        if(m_window)return true;
        m_module=module;
        WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=WindowProc;cls.hInstance=module;cls.lpszClassName=ClassName;
        if(!RegisterClassExW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
        m_registered=true;
        m_window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_TRANSPARENT|WS_EX_LAYERED,
            ClassName,L"RuneSchema diagnostic notifications",WS_POPUP,0,0,420,90,nullptr,nullptr,module,this);
        if(!m_window)return false;
        m_logo=static_cast<HICON>(LoadImageW(module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,32,32,0));
        if(m_mode!=Mode::Startup)SetLayeredWindowAttributes(m_window,ColorKey,244,LWA_COLORKEY|LWA_ALPHA);
        return true;
    }
public:
    bool HasStartupLogos()const{return !m_startupRune.Pixels.empty()||!m_startupGame.Pixels.empty();}
    void StartupLogos(StartupPng rune,StartupPng game,StartupPng wheel){
        m_startupRune=std::move(rune);m_startupGame=std::move(game);m_startupWheel=std::move(wheel);
    }
#if defined(HELPY_PREVIEW)
    bool PreviewStartupAlpha(const HelpyImGuiTheme::Theme& theme){
        m_theme=theme;m_fps="Registering game content\nPreview loading tip";
        m_startupLayout=LoadingIndicator::FitStartup(1280,720,1.f,1.f);
        m_width=int(std::lround(m_startupLayout.Width));
        for(double elapsed:{.25,4.25}){
            StartupSurface image(m_width,int(std::ceil(m_startupLayout.Height)));
            if(!image.Pixels)return false;
            PaintStartup(image,elapsed);
            if(image.Pixels[0]||image.Pixels[image.Width-1]||
               image.Pixels[size_t(image.Height-1)*image.Width]||image.Pixels[size_t(image.Height)*image.Width-1])return false;
            const auto visible=[&](int top,int bottom){
                const auto begin=image.Pixels+size_t(std::clamp(top,0,image.Height))*image.Width;
                const auto end=image.Pixels+size_t(std::clamp(bottom,0,image.Height))*image.Width;
                return std::any_of(begin,end,[](uint32_t pixel){return (pixel>>24)>0;});
            };
            if(!visible(int(m_startupLayout.LogoY-20),int(m_startupLayout.LogoY+20))||
               !visible(int(m_startupLayout.WheelY-8),int(m_startupLayout.WheelY+8))||
               !visible(int(m_startupLayout.TextY),image.Height))return false;
        }
        return true;
    }
#endif
    void Hide(){if(m_window&&m_showing)ShowWindow(m_window,SW_HIDE);m_showing=false;}
    void Close(){if(m_window){DestroyWindow(m_window);m_window=nullptr;}if(m_logo){DestroyIcon(m_logo);m_logo=nullptr;}StartupLogos({},{},{});if(m_registered){UnregisterClassW(ClassName,m_module);m_registered=false;}m_showing=false;m_game=nullptr;m_monitor=nullptr;m_nextUpdate={};}
    bool TakeModsRequest(){const bool requested=m_viewMods;m_viewMods=false;return requested;}
    void Update(HINSTANCE module,HWND game,const DiagnosticToastFeed::Feed& feed,const HelpyImGuiTheme::Theme& theme,bool show,std::string_view fps={},const ServerModVersions::Summary& versions={},Mode mode=Mode::Notifications,const HelpyHudLayout::Settings& hud={}){
        if(!show||!game||!IsWindow(game)||(feed.Empty()&&fps.empty())){Hide();return;}
        const auto label=mode==Mode::Coordinates?HelpyHudLayout::CoordinateText(std::string(fps),hud):std::string(fps);const bool fpsChanged=m_fps!=label;
        const bool modeChanged=m_mode!=mode;
        if(m_window&&modeChanged&&(m_mode==Mode::Startup||mode==Mode::Startup)){
            DestroyWindow(m_window);m_window=nullptr;m_showing=false;
            if(m_logo){DestroyIcon(m_logo);m_logo=nullptr;}
        }
        m_mode=mode;
        const bool versionsChanged=m_versions!=versions;
        const auto now=std::chrono::steady_clock::now();if(m_showing&&now<m_nextUpdate&&!modeChanged&&!fpsChanged&&!versionsChanged&&m_cards.size()==feed.Visible().size())return;
        if(m_mode==Mode::Startup&&(!m_showing||modeChanged)){m_started=now;BOOL animate=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0);m_animate=animate!=FALSE;}
        m_nextUpdate=now+std::chrono::milliseconds(m_mode==Mode::Startup?(m_animate?16:500):feed.Empty()?std::clamp(theme.fpsRefreshMs,500,2000):100);
        if(!Ensure(module))return;
        m_fps=label;
        m_versions=versions;
        bool cardsChanged=m_cards.size()!=feed.Visible().size();
        if(!cardsChanged){size_t i=0;for(const auto& item:feed.Visible()){
            const auto& prior=m_cards[i++];
            if(prior!=item){cardsChanged=true;break;}
        }}
        if(cardsChanged)m_cards.assign(feed.Visible().begin(),feed.Visible().end());
        const bool clickable=std::any_of(m_cards.begin(),m_cards.end(),[](const auto& card){return DiagnosticToastPresentation::Format(card).WorldMods;});
        const auto style=GetWindowLongPtrW(m_window,GWL_EXSTYLE),wanted=clickable?style&~WS_EX_TRANSPARENT:style|WS_EX_TRANSPARENT;
        if(style!=wanted)SetWindowLongPtrW(m_window,GWL_EXSTYLE,wanted);
        const float priorScale=m_scale;
        const float priorFont=m_theme.fontScale,priorDensity=m_theme.density;
        const std::string priorFamily=m_theme.fontFamily;
        const auto colorChanged=[&](const ImVec4& a,const ImVec4& b){return a.x!=b.x||a.y!=b.y||a.z!=b.z||a.w!=b.w;};
        const bool themeChanged=colorChanged(m_theme.panelAlt,theme.panelAlt)||colorChanged(m_theme.border,theme.border)||colorChanged(m_theme.text,theme.text)||colorChanged(m_theme.textDisabled,theme.textDisabled)||colorChanged(m_theme.accent,theme.accent)||colorChanged(m_theme.error,theme.error)||colorChanged(m_theme.warning,theme.warning)||colorChanged(m_theme.info,theme.info)||m_theme.childRounding!=theme.childRounding;
        m_theme=theme;
        const auto currentMonitor=MonitorFromWindow(game,MONITOR_DEFAULTTONEAREST);
        if(m_game!=game||m_monitor!=currentMonitor){m_game=game;m_monitor=currentMonitor;m_dpi=GetDpiForWindow(game);}
        const UINT dpi=m_dpi;
        const float dpiScale=theme.dpiAware&&dpi?float(dpi)/96.0f:1.0f;
        m_scale=std::clamp(dpiScale*theme.userScale,0.75f,3.0f);
        m_gap=std::max(4,Spacing(7));
        RECT client{};GetClientRect(game,&client);
        POINT clientOrigin{client.left,client.top};ClientToScreen(game,&clientOrigin);
        MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(game,MONITOR_DEFAULTTONEAREST),&monitor);
        const int margin=Spacing(14);
        // Normalize Win32 LONG coordinates once. All subsequent geometry
        // uses int-only helpers, preventing std::max/clamp template mismatches.
        const auto bounds=HelpyOverlayGeometry::Normalize(clientOrigin,client,monitor.rcWork);
        const int clientWidth=bounds.ClientWidth;
        const int oldWidth=m_width;
        const int availableWidth=HelpyOverlayGeometry::AvailableWidth(bounds,margin);
        if(availableWidth<180){Hide();return;}
        const int maxHeight=HelpyOverlayGeometry::MaxHeight(bounds,margin);
        if(m_mode==Mode::Startup)m_startupLayout=LoadingIndicator::FitStartup(float(std::min(clientWidth,availableWidth)),float(std::min(int(client.bottom),maxHeight)),m_scale,theme.fontScale);
        m_width=m_mode==Mode::Startup?int(std::lround(m_startupLayout.Width)):std::min(static_cast<int>(std::lround((m_mode==Mode::Coordinates?600:m_cards.empty()?160:400)*m_scale)),availableWidth);
        if(m_mode==Mode::Coordinates&&hud.Minimap&&hud.CoordinatesBelowMap)m_width=std::min(m_width,int(HelpyHudLayout::Map(float(client.right),float(client.bottom),hud).W));
        const bool resized=oldWidth!=m_width||std::abs(priorScale-m_scale)>0.001f||priorFont!=m_theme.fontScale||
            priorDensity!=m_theme.density||priorFamily!=m_theme.fontFamily;
        if(cardsChanged||versionsChanged||resized||m_layouts.empty()){
            const HDC dc=CreateCompatibleDC(nullptr);
            if(!dc){Hide();return;}
            MeasureCards(dc);DeleteDC(dc);
        }
        // Never draw past the monitor work area. If all cards cannot fit,
        // reserve a visibly labeled summary instead of silently hiding them.
        size_t hidden=0;
        while(m_cards.size()>1&&StackHeight()>maxHeight){m_cards.pop_back();m_layouts.pop_back();++hidden;}
        if(hidden){
            DiagnosticToastFeed::Notice extra;extra.Summary=true;
            extra.Text=std::to_string(hidden)+" other notifications; full entries in Live Log";
            m_cards.push_back(extra);
            const HDC dc=CreateCompatibleDC(nullptr);
            if(dc){MeasureCards(dc);}
            while(dc&&m_cards.size()>2&&StackHeight()>maxHeight){
                m_cards.erase(m_cards.end()-2);++hidden;
                m_cards.back().Text=std::to_string(hidden)+" other notifications; full entries in Live Log";
                MeasureCards(dc);
            }
            if(dc)DeleteDC(dc);
            else {Hide();return;}
        }
        // Very short/low-resolution windows cannot safely host even two
        // cards. Show only a compact handoff to Live Log rather than cropping.
        if(StackHeight()>maxHeight){
            m_cards.clear();
            DiagnosticToastFeed::Notice compact;compact.Summary=true;
            compact.Text="See HELPY Live Log for notifications";
            m_cards.push_back(std::move(compact));
            const HDC dc=CreateCompatibleDC(nullptr);
            if(!dc){Hide();return;}
            MeasureCards(dc);DeleteDC(dc);
            if(StackHeight()>maxHeight){Hide();return;}
        }
        const int actualHeight=StackHeight();
        const auto placement=HelpyOverlayGeometry::Place(bounds,m_width,actualHeight,margin);
        int x=m_mode==Mode::Startup?std::clamp(int(clientOrigin.x+(client.right-m_width)/2),bounds.MonitorLeft,std::max(bounds.MonitorLeft,bounds.MonitorRight-m_width)):placement.X;
        int y=m_mode==Mode::Startup?std::clamp(int(clientOrigin.y+(client.bottom-actualHeight)/2),bounds.MonitorTop,std::max(bounds.MonitorTop,bounds.MonitorBottom-actualHeight)):m_mode==Mode::Coordinates?std::clamp(int(clientOrigin.y+client.bottom)-actualHeight-margin,bounds.MonitorTop,std::max(bounds.MonitorTop,bounds.MonitorBottom-actualHeight)):placement.Y;
        if(hud.Custom){const auto box=m_mode==Mode::Coordinates?HelpyHudLayout::Coordinates(float(client.right),float(client.bottom),float(m_width),float(actualHeight),hud):HelpyHudLayout::Place(float(client.right),float(client.bottom),float(m_width),float(actualHeight),m_mode==Mode::Startup?hud.Loading:hud.Notifications);x=std::clamp(int(clientOrigin.x+box.X),bounds.MonitorLeft,std::max(bounds.MonitorLeft,bounds.MonitorRight-m_width));y=std::clamp(int(clientOrigin.y+box.Y),bounds.MonitorTop,std::max(bounds.MonitorTop,bounds.MonitorBottom-actualHeight));}
        if(m_mode==Mode::Startup){
            StartupSurface surface(m_width,actualHeight);
            if(!surface.Pixels){Hide();return;}
            PaintStartup(surface,std::chrono::duration<double>(now-m_started).count());
            POINT position{x,y},origin{};
            SIZE size{m_width,actualHeight};
            BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            if(!UpdateLayeredWindow(m_window,nullptr,&position,&size,surface.Dc,&origin,0,&blend,ULW_ALPHA)){
                Hide();return;
            }
            if(!m_showing)ShowWindow(m_window,SW_SHOWNOACTIVATE);
            m_showing=true;
            return;
        }
        const bool windowWasVisible=m_showing;
        RECT old{};GetWindowRect(m_window,&old);
        const bool frameChanged=!windowWasVisible||old.left!=x||old.top!=y||static_cast<int>(old.right-old.left)!=m_width||static_cast<int>(old.bottom-old.top)!=actualHeight;
        if(frameChanged)SetWindowPos(m_window,HWND_TOPMOST,x,y,m_width,actualHeight,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        m_showing=true;
        if(cardsChanged||versionsChanged||fpsChanged||resized||frameChanged||themeChanged||m_mode==Mode::Startup){
            InvalidateRect(m_window,nullptr,FALSE);UpdateWindow(m_window);
        }
    }
    ~HelpyDiagnosticOverlay(){Close();}
};
}
