#pragma once
#include <array>
#include <span>
#include <cmath>
#include <algorithm>

// H1 cross-spectrum transfer estimate. It is meaningful only in excited,
// sufficiently coherent bands; missing information is never invented.
class ToneBoyTransferAnalysis
{
public:
    struct Estimate { float gainDb{}, confidence{}; };
    void clear() { bins.fill({}); }
    void update(std::span<const float> input,std::span<const float> output)
    {
        if(input.size()<8192 || output.size()<8192) return;
        constexpr double scale=1.0/2048;
        for(size_t i=1;i<bins.size();++i)
        {
            const double xr=input[2*i]*scale,xi=input[2*i+1]*scale,yr=output[2*i]*scale,yi=output[2*i+1]*scale;
            const double xx=xr*xr+xi*xi,yy=yr*yr+yi*yi,re=xr*yr+xi*yi,im=xr*yi-xi*yr;
            auto& b=bins[i];
            if(!std::isfinite(xx+yy+re+im)) { b={}; continue; }
            b.active=xx>=1e-7?std::min(255,b.active+1):0;
            const float a=b.active==1?1.0f:0.28f;
            b.xx+=a*(float(xx)-b.xx); b.yy+=a*(float(yy)-b.yy);
            b.re+=a*(float(re)-b.re); b.im+=a*(float(im)-b.im); b.current=float(xx);
        }
    }
    Estimate estimate(double frequency,double rate) const
    {
        if(rate<=0 || frequency<20 || frequency>=rate*0.49) return {};
        const int index=std::clamp(int(std::round(frequency*8192/rate)),1,4095);
        const auto& b=bins[size_t(index)];
        if(b.active<3 || b.current<1e-7f || b.xx<1e-12f || b.yy<1e-12f) return {};
        const double cross=double(b.re)*b.re+double(b.im)*b.im;
        const double coherence=std::clamp(cross/(double(b.xx)*b.yy),0.0,1.0);
        if(coherence<0.70) return {};
        const double gain=10*std::log10(std::max(1e-20,cross/(double(b.xx)*b.xx)));
        if(!std::isfinite(gain) || gain< -96 || gain>48) return {};
        const auto excitation=std::clamp((10*std::log10(double(b.current))+70)/25,0.0,1.0);
        return {float(gain),float(std::clamp((coherence-0.70)/0.25,0.0,1.0)*excitation)};
    }
private:
    struct Bin { float xx{},yy{},re{},im{},current{}; int active{}; };
    std::array<Bin,4096> bins{};
};

// Passive analysis only. No test oscillator or processing is inserted in the audio path.
struct ToneBoyHarmonicResult
{
    enum class Status { silence, needsTone, valid };
    Status status{Status::silence};
    double frequency{}, thdDb{-120}, thdnDb{-120};
    std::array<float,10> harmonicsDb=[] { std::array<float,10> a{}; a.fill(-120); return a; }();
    int harmonicCount{};
};

class ToneBoyHarmonicAnalysis
{
    struct Fit { double dc{}, sine{}, cosine{}, error{}, power{}; };
    static Fit fit(std::span<const float> data,double frequency,double rate)
    {
        const double angle=6.2831853071795864769*frequency/rate;
        const double ds=std::sin(angle),dc=std::cos(angle);
        double s=0,c=1,ss=0,cc=0,sc=0,sx=0,cx=0,sumS=0,sumC=0,sumX=0,power=0;
        for(float x:data)
        {
            ss+=s*s; cc+=c*c; sc+=s*c; sx+=s*x; cx+=c*x; sumS+=s; sumC+=c; sumX+=x; power+=double(x)*x;
            const double next=s*dc+c*ds; c=c*dc-s*ds; s=next;
        }
        double a[3][4]{{double(data.size()),sumS,sumC,sumX},{sumS,ss,sc,sx},{sumC,sc,cc,cx}};
        for(int column=0;column<3;++column)
        {
            int pivot=column;
            for(int row=column+1;row<3;++row) if(std::abs(a[row][column])>std::abs(a[pivot][column])) pivot=row;
            for(int k=0;k<4;++k) std::swap(a[column][k],a[pivot][k]);
            const auto divisor=a[column][column];
            if(std::abs(divisor)<1e-12) return {};
            for(int k=column;k<4;++k) a[column][k]/=divisor;
            for(int row=0;row<3;++row) if(row!=column)
            {
                const auto factor=a[row][column];
                for(int k=column;k<4;++k) a[row][k]-=factor*a[column][k];
            }
        }
        Fit result{a[0][3],a[1][3],a[2][3],0,power};
        // A second pass avoids catastrophic cancellation for very clean tones.
        s=0;c=1;
        for(float x:data) { const double e=x-result.dc-result.sine*s-result.cosine*c; result.error+=e*e; const double next=s*dc+c*ds; c=c*dc-s*ds; s=next; }
        return result;
    }
public:
    static ToneBoyHarmonicResult analyse(std::span<const float> input,std::span<const float> output,double rate)
    {
        ToneBoyHarmonicResult result;
        if(input.size()!=output.size() || input.size()<128 || !std::isfinite(rate) || rate<=0) return result;
        double energy=0,mean=0;
        for(size_t i=0;i<input.size();++i) { if(!std::isfinite(input[i]) || !std::isfinite(output[i])) return result; mean+=input[i]; energy+=double(input[i])*input[i]; }
        if(energy/double(input.size())<1e-8) return result;
        mean/=double(input.size());
        energy-=double(input.size())*mean*mean;
        if(energy/double(input.size())<1e-8) return result;
        double first=0,last=0; int crossings=0;
        for(size_t i=1;i<input.size();++i)
        {
            const double previous=input[i-1]-mean,current=input[i]-mean;
            if(previous<=0 && current>0) { const double position=double(i-1)-previous/(current-previous); if(crossings++==0) first=position; last=position; }
        }
        result.status=ToneBoyHarmonicResult::Status::needsTone;
        if(crossings<8 || last<=first) return result;
        double frequency=double(crossings-1)*rate/(last-first);
        if(frequency<8*rate/double(input.size()) || frequency>=rate*0.20) return result;
        // Refine frequency using the input, not the distorted output. This avoids
        // mistaking H2 for the fundamental and suppresses non-coherent-window leakage.
        double step=rate/double(input.size())*0.10;
        for(int iteration=0;iteration<7;++iteration)
        {
            const auto left=fit(input,frequency-step,rate).error;
            const auto centre=fit(input,frequency,rate).error;
            const auto right=fit(input,frequency+step,rate).error;
            const auto curvature=left-2*centre+right;
            if(curvature>1e-24) frequency+=std::clamp(0.5*(left-right)/curvature,-1.0,1.0)*step;
            step*=0.25;
        }
        const auto inputFit=fit(input,frequency,rate);
        if(inputFit.error/energy>0.0025) return result; // Reject music, multitone and noisy/unstable excitation.
        const auto fundamental=fit(output,frequency,rate);
        const double amplitudeSq=fundamental.sine*fundamental.sine+fundamental.cosine*fundamental.cosine;
        if(amplitudeSq<1e-10) return result;
        const auto toDb=[](double ratio){return 10*std::log10(std::max(1e-12,ratio));};
        result.frequency=frequency; result.harmonicsDb[0]=0; result.harmonicCount=1;
        result.thdnDb=toDb(fundamental.error/(double(output.size())*amplitudeSq*0.5));
        // Remove fundamental and DC before projecting harmonics. Unlike raw FFT
        // bins, this works for off-bin tones and does not count H1 leakage as THD.
        const double angle=6.2831853071795864769*frequency/rate,ds=std::sin(angle),dc=std::cos(angle);
        std::array<double,10> hs{},hc{},hss{},hcc{},hsc{},hxS{},hxC{},stepS{},stepC{};
        hc.fill(1);
        for(int h=2;h<=10;++h) { stepS[size_t(h-1)]=std::sin(angle*h); stepC[size_t(h-1)]=std::cos(angle*h); }
        double s=0,c=1;
        for(float x:output)
        {
            const double residual=x-fundamental.dc-fundamental.sine*s-fundamental.cosine*c;
            for(int h=2;h<=10 && frequency*h<rate*0.49;++h)
            {
                const size_t j=size_t(h-1); hss[j]+=hs[j]*hs[j]; hcc[j]+=hc[j]*hc[j]; hsc[j]+=hs[j]*hc[j]; hxS[j]+=residual*hs[j]; hxC[j]+=residual*hc[j];
                const double next=hs[j]*stepC[j]+hc[j]*stepS[j]; hc[j]=hc[j]*stepC[j]-hs[j]*stepS[j]; hs[j]=next;
            }
            const double next=s*dc+c*ds; c=c*dc-s*ds; s=next;
        }
        double harmonicEnergy=0;
        for(int h=2;h<=10 && frequency*h<rate*0.49;++h)
        {
            const size_t j=size_t(h-1); const auto determinant=hss[j]*hcc[j]-hsc[j]*hsc[j];
            if(determinant<=1e-12) break;
            const auto a=(hxS[j]*hcc[j]-hxC[j]*hsc[j])/determinant,b=(hxC[j]*hss[j]-hxS[j]*hsc[j])/determinant;
            const double relative=(a*a+b*b)/amplitudeSq;
            result.harmonicsDb[j]=float(toDb(relative)); harmonicEnergy+=relative; ++result.harmonicCount;
        }
        result.thdDb=toDb(harmonicEnergy); result.status=ToneBoyHarmonicResult::Status::valid;
        return result;
    }
};
