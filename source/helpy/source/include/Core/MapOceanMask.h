#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace PS::MapOceanMask {
// Cartographic warning only: the native map's transparent sea/absent-land
// artwork is not a collision, swimming or streamed-world safety contract.
struct Run {int Y=0,Left=0,Right=0;};
struct Mask {
    static constexpr int Side=128;
    std::vector<uint8_t> Ocean;
    std::vector<Run> Runs;
    bool Contains(double u,double v)const{
        if(Ocean.size()!=Side*Side||!std::isfinite(u)||!std::isfinite(v)||u<0||u>=1||v<0||v>=1)return false;
        return Ocean[int(v*Side)*Side+int(u*Side)]!=0;
    }
    static Mask Build(const std::vector<unsigned char>& rgba,int width,int height){
        Mask out;if(width<Side||height<Side||width>2048||height>2048||rgba.size()!=size_t(width)*height*4)return out;
        std::vector<uint8_t> water(Side*Side);out.Ocean.resize(Side*Side);
        for(int y=0;y<Side;++y)for(int x=0;x<Side;++x){bool sea=true;
            // Require every sampled point in a cell to be outside opaque land.
            // This leaves ambiguous coastline yellow rather than coloring it red.
            for(const int dy:{1,4,7})for(const int dx:{1,4,7}){const int px=std::min(width-1,(x*8+dx)*width/(Side*8)),py=std::min(height-1,(y*8+dy)*height/(Side*8));
                const auto i=(size_t(py)*width+px)*4;const int r=rgba[i],g=rgba[i+1],b=rgba[i+2],a=rgba[i+3];
                sea=sea&&(a<8||(a<192&&g>=r+8&&b>=r+8));}
            water[y*Side+x]=sea;
        }
        std::vector<int> queue;queue.reserve(Side*Side);
        const auto push=[&](int x,int y){const int i=y*Side+x;if(water[i]&&!out.Ocean[i]){out.Ocean[i]=1;queue.push_back(i);}};
        for(int i=0;i<Side;++i){push(i,0);push(i,Side-1);push(0,i);push(Side-1,i);}
        for(size_t at=0;at<queue.size();++at){const int i=queue[at],x=i%Side,y=i/Side;if(x)push(x-1,y);if(x+1<Side)push(x+1,y);if(y)push(x,y-1);if(y+1<Side)push(x,y+1);}
        for(int y=0;y<Side;++y)for(int x=0;x<Side;){if(!out.Ocean[y*Side+x]){++x;continue;}const int left=x;while(x<Side&&out.Ocean[y*Side+x])++x;out.Runs.push_back({y,left,x});}
        return out;
    }
};
}
