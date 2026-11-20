// Reproducible portfolio experiments; outputs never overwrite Image/ assets.
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>
#include "miniISP/InputData.h"
#include "miniISP/BayerUtils.h"
#include "miniISP/BlackLevelCorrection.h"
#include "miniISP/DefectivePixelCorrection.h"
#include "miniISP/LensShadingCorrection.h"
#include "miniISP/AutoWhiteBalance.h"
#include "miniISP/Demosaic.h"
#include "miniISP/ColorCalibration.h"
#include "miniISP/ImageMath.h"
#include "miniISP/HighDynamicRange.h"

using namespace mini_isp;
namespace fs = std::filesystem;
const fs::path out="docs/career/comparisons/data";
std::ofstream metrics;
void metric(const std::string& group,const std::string& key,double value){
    metrics<<group<<","<<key<<","<<std::setprecision(12)<<value<<"\n";
}
RawImage raw(const std::string& path){RawImage r;if(!ReadDNG(path,r))throw std::runtime_error(path);return r;}
std::vector<uint16_t> blc(const RawImage& r){std::vector<uint16_t> v;ApplyBlackLevelCorrection16_v1(r.visible,v,r.width,r.height,r.blackLevel);return v;}
void save(const std::string& name,const std::vector<RGBFormat>& v,int w,int h){if(!SavePPM((out/(name+".ppm")).string(),v,w,h))throw std::runtime_error(name);}
std::vector<RGBFormat> read(const std::string& path,int& w,int& h){std::vector<RGBFormat> v;int max;if(!ReadPPM(path,v,w,h,max)||max!=255)throw std::runtime_error(path);return v;}
std::vector<RGBFormat> crop(const std::vector<RGBFormat>& v,int w,int h,int x,int y,int cw,int ch){
    if(x<0||y<0||x+cw>w||y+ch>h)throw std::runtime_error("Invalid ROI");
    std::vector<RGBFormat> c;for(int yy=y;yy<y+ch;++yy)c.insert(c.end(),v.begin()+yy*w+x,v.begin()+yy*w+x+cw);return c;
}
uint8_t encode(double x){return static_cast<uint8_t>(std::lround(255*LinearToSRGB(static_cast<float>(std::clamp(x,0.,1.)))));}
std::vector<RGBFormat> preview(const std::vector<uint16_t>& b,const RawImage& r){
    std::vector<uint16_t> corrected;ApplyBlackLevelCorrection16_v1(b,corrected,r.width,r.height,r.blackLevel);
    std::vector<RGBFormat16> v;DemosaicBilinear16(corrected,v,r.width,r.height,r.pattern,r.whiteLevel-r.blackLevel);
    std::vector<RGBFormat> p(v.size());double scale=r.whiteLevel-r.blackLevel;
    for(size_t i=0;i<v.size();++i)p[i]={encode(v[i].r/scale),encode(v[i].g/scale),encode(v[i].b/scale)};return p;
}
void dpc(){
    auto r=raw("Image/black.DNG");auto defects=GenerateRandomHotPixel(r.width,r.height,200);
    std::vector<uint16_t> injected,corrected;
    InjectSyntheticDefect16(r.visible,injected,r.width,r.height,r.blackLevel,r.whiteLevel,defects);
    DefectivePixelCorrection16_v1(injected,corrected,r.width,r.height,r.pattern,500);
    std::vector<bool> mask(r.visible.size());double e0=0,e1=0;size_t detected=0,other=0;
    for(auto& d:defects){size_t i=d.y*r.width+d.x;mask[i]=true;e0+=std::abs(int(injected[i])-int(r.visible[i]));e1+=std::abs(int(corrected[i])-int(r.visible[i]));detected+=corrected[i]!=injected[i];}
    for(size_t i=0;i<mask.size();++i)if(!mask[i]&&corrected[i]!=r.visible[i])++other;
    auto d=*std::find_if(defects.begin(),defects.end(),[&](auto& d){return d.x>32&&d.y>32&&d.x<r.width-32&&d.y<r.height-32;});
    metric("dpc","x",d.x-32);metric("dpc","y",d.y-32);metric("dpc","size",64);
    metric("dpc","injected",defects.size());metric("dpc","replaced",detected);
    metric("dpc","mae_before_DN",e0/defects.size());metric("dpc","mae_after_DN",e1/defects.size());metric("dpc","noninjected_changes",other);
    int k=0;for(auto* b:{&r.visible,&injected,&corrected}){auto p=preview(*b,r);save("dpc_"+std::to_string(k++),crop(p,r.width,r.height,d.x-32,d.y-32,64,64),64,64);}
}
double cv(const std::vector<double>& a){double mean=std::accumulate(a.begin(),a.end(),0.)/a.size(),s=0;for(double x:a)s+=(x-mean)*(x-mean);return 100*std::sqrt(s/a.size())/mean;}
void lsc(){
    auto r=raw("Image/lsc_flatfield_synthetic.dng");auto input=blc(r);LensShadingCorrectionMap map;
    BuildLensShadingMap(input,r.width,r.height,r.pattern,17,13,map);std::vector<uint16_t> v1,v2;
    ApplyLensShadingCorrection(input,v1,r.width,r.height,r.pattern,r.whiteLevel-r.blackLevel,map);
    ApplyLensShadingCorrectionV2(input,v2,r.width,r.height,r.pattern,r.whiteLevel-r.blackLevel,map);
    std::ofstream profile(out/"lsc_profile.csv");profile<<"x,input,v1,v2\n";
    for(int x=0;x<r.width;x+=16){profile<<x;for(auto* a:{&input,&v1,&v2}){double s=0;int n=0;for(int y=r.height/2-8;y<r.height/2+8;++y)for(int xx=x;xx<std::min(x+16,r.width);++xx)if(GetCFAColor(r.pattern,xx,y)==CFAColor::Green){s+=(*a)[y*r.width+xx];++n;}profile<<","<<s/n;}profile<<"\n";}
    int k=0;for(auto* a:{&input,&v1,&v2}){
        std::array<std::vector<double>,4> phase;std::vector<RGBFormat> p(a->size());
        for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){size_t i=y*r.width+x;phase[(y%2)*2+x%2].push_back((*a)[i]);uint8_t c=static_cast<uint8_t>(std::lround(std::min(255.,(*a)[i]*255./4031)));p[i]={c,c,c};}
        double meanCV=0;for(auto& v:phase)meanCV+=cv(v)/4;
        metric("lsc","cv_"+std::to_string(k),meanCV);save("lsc_"+std::to_string(k++),p,r.width,r.height);
    }
}
void demosaic(){
    int w,h;auto reference=read("Image/Tree.ppm",w,h);std::vector<uint8_t> b;RGBToBayerRaw(reference,b,w,h);
    std::vector<RGBFormat> bilinear,edge;DemosaicByBayer_Optimal(b,bilinear,w,h);DemosaicByBayer_EdgeAwareV2(b,edge,w,h);
    save("demosaic_reference",reference,w,h);save("demosaic_bilinear",bilinear,w,h);save("demosaic_edge",edge,w,h);
    metric("demosaic","width",w);metric("demosaic","height",h);
    for(auto pair:{std::make_pair("bilinear",&bilinear),std::make_pair("edge",&edge)}){
        double ss=0,sa=0;size_t n=0;
        for(int y=2;y<h-2;++y)for(int x=2;x<w-2;++x){int i=y*w+x;auto a=reference[i],b=(*pair.second)[i];for(int d:{int(a.r)-int(b.r),int(a.g)-int(b.g),int(a.b)-int(b.b)}){ss+=d*d;sa+=std::abs(d);++n;}}
        metric("demosaic",std::string(pair.first)+"_PSNR_dB",10*std::log10(255.*255/(ss/n)));metric("demosaic",std::string(pair.first)+"_MAE_code",sa/n);
    }
}
void ccm(){
    auto r=raw("Image/colorChart400-1:15.DNG");auto input=blc(r);double white=r.whiteLevel-r.blackLevel;
    ColorChartCorners corners{{1240,1060},{2825,1030},{1230,2170},{2845,2200}};
    auto patches=GetColorChartPatchColors(input,r.width,r.height,r.pattern,corners);double sr=0,sg=0,sb=0;
    for(int i=1;i<5;++i){sr+=patches[i].r;sg+=patches[i].g;sb+=patches[i].b;}
    double gr=sg/sr,gb=sg/sb;auto balanced=patches;for(auto& p:balanced){p.r*=gr;p.b*=gb;}
    const std::array<PatchColorDouble,24> encoded{{{94,28,13},{241,149,108},{97,119,171},{90,103,39},{164,131,196},{140,253,153},{255,116,21},{7,47,122},{222,29,42},{69,0,68},{187,253,19},{255,142,0},{0,0,142},{64,173,38},{203,0,0},{255,217,0},{207,3,124},{0,148,189},{255,255,255},{249,249,249},{180,180,180},{117,117,117},{53,53,53},{0,0,0}}};
    auto targets=encoded;for(auto& p:targets){p.r=AdobeRGBToLinear(p.r/255);p.g=AdobeRGBToLinear(p.g/255);p.b=AdobeRGBToLinear(p.b/255);}
    auto matrix=CalculateCCM(balanced,targets,white);Matrix3x3 identity{{{1,0,0},{0,1,0},{0,0,1}}};
    std::ofstream patchCSV(out/"ccm_patch_errors.csv");patchCSV<<"measured_index,reference_index,identity_linear_RMSE,fitted_linear_RMSE\n";
    double sum0=0,sum1=0;
    for(int i=0;i<24;++i){if(i==0||i==5)continue;PatchColorDouble p{balanced[i].r/white,balanced[i].g/white,balanced[i].b/white};auto c=ApplyCCM(p,matrix),t=targets[23-i];double e0=std::pow(p.r-t.r,2)+std::pow(p.g-t.g,2)+std::pow(p.b-t.b,2),e1=std::pow(c.r-t.r,2)+std::pow(c.g-t.g,2)+std::pow(c.b-t.b,2);sum0+=e0;sum1+=e1;patchCSV<<i<<","<<23-i<<","<<std::sqrt(e0/3)<<","<<std::sqrt(e1/3)<<"\n";}
    metric("ccm","fit_rmse_before",std::sqrt(sum0/66));metric("ccm","fit_rmse_after",std::sqrt(sum1/66));metric("ccm","gainR",gr);metric("ccm","gainB",gb);
    std::vector<uint16_t> awb;ApplyWhiteBalanceGains16(input,awb,r.width,r.height,r.pattern,gr,1,gb,white);
    size_t capped=0;for(auto v:awb)capped+=v>white;metric("ccm","awb_samples_above_white_pct",100.*capped/awb.size());
    std::vector<RGBFormat16> rgb;DemosaicBilinear16(awb,rgb,r.width,r.height,r.pattern,white);
    int k=0;for(auto* m:{&identity,&matrix}){
        std::vector<RGBFloatFormat> f;ApplyCCMImage(rgb,f,*m,white);size_t clipped=0;
        for(auto& p:f){auto s=LinearAdobeRGBToLinearSRGB(p);clipped+=(s.r<0||s.r>1);clipped+=(s.g<0||s.g>1);clipped+=(s.b<0||s.b>1);}
        metric("ccm","out_of_srgb_channels_pct_"+std::to_string(k),100.*clipped/(f.size()*3));
        std::vector<RGBFormat> p;ConvertLinearAdobeRGBToSRGB(f,p);
        save("ccm_"+std::to_string(k++),crop(p,r.width,r.height,1200,990,1700,1250),1700,1250);
    }
    std::ofstream mat(out/"ccm_matrix.csv");for(auto row:matrix)mat<<row[0]<<","<<row[1]<<","<<row[2]<<"\n";
}
void hdr(){
    int w,h,w1,h1,w2,h2;auto lo=read("Image/srgb_hdr/waterfall_low_srgb.ppm",w,h),mi=read("Image/srgb_hdr/waterfall_mid_srgb.ppm",w1,h1),hi=read("Image/srgb_hdr/waterfall_high_srgb.ppm",w2,h2);
    if(w!=w1||w!=w2||h!=h1||h!=h2)throw std::runtime_error("HDR geometry mismatch");
    std::vector<RGBFloatFormat> rad;std::vector<RGBFormat> result,fusion;
    ApplyHighDynamicRange(lo,mi,hi,rad,w,h,1.f/6,1.3f,5.f);ToneMapReinhard(rad,result,.18f);AdaptiveExposureFusion(lo,mi,hi,fusion,w,h,8,8);
    save("hdr",result,w,h);save("fusion",fusion,w,h);
    for(auto pair:{std::make_pair("mid",&mi),std::make_pair("hdr",&result),std::make_pair("fusion",&fusion)}){size_t white=0,black=0;for(auto p:*pair.second){white+=std::max({p.r,p.g,p.b})==255;black+=p.r==0&&p.g==0&&p.b==0;}metric("hdr",std::string(pair.first)+"_any255_pct",100.*white/pair.second->size());metric("hdr",std::string(pair.first)+"_all0_pct",100.*black/pair.second->size());}
}
int main(){try{fs::create_directories(out);metrics.open(out/"metrics.csv");metrics<<"experiment,metric,value\n";dpc();std::cout<<"DPC done\n";lsc();std::cout<<"LSC done\n";demosaic();std::cout<<"Demosaic done\n";ccm();std::cout<<"CCM done\n";hdr();std::cout<<"HDR done\n";}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
