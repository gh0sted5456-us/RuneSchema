#include "Core/ItemBayLayout.h"
#include <limits>
#include <stdexcept>
#include <iostream>
static void Require(bool ok){if(!ok)throw std::runtime_error("Item bay sizing regression");}
int main(){
    using namespace PS::ItemBayLayout;
    Require(DefaultColumns==5&&DefaultRows==4);
    Require(Columns(1200,132,8,DefaultColumns)==5);
    Require(Columns(692,132,8,5)==5);
    Require(Columns(691,132,8,5)==4);
    Require(Columns(400,132,8,5)==2);
    Require(Columns(80,132,8,5)==1);
    Require(Columns(10000,132,8,99)==MaxColumns);
    Require(Columns(1200,132,8,3)==3);
    Require(Columns(std::numeric_limits<float>::quiet_NaN(),132,8,5)==1);
    Require(RowsHeight(4,116,6,6)==494);
    Require(RowsHeight(1,116,6,6)==128);
    Require(PaneHeight(494,700,140)==494);
    Require(PaneHeight(494,300,140)==300);
    Require(PaneHeight(494,90,140)==90);
    Require(PaneHeight(494,0,140)==1);
    Require(PaneHeight(std::numeric_limits<float>::quiet_NaN(),700,140)==140);
    const auto preferred=RowsHeight(DefaultRows,116,6,6);
    Require(PaneHeight(preferred,180,140)==180&&PaneHeight(preferred,700,140)==preferred);
    std::cout<<"PASS: 5x4 defaults, adjustable grid, narrow windows and restored row height.\n";
}
