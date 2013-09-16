cbuffer CB : register( b0 )
{
	float tm : packoffset( c0 );
};

float flr(float3 p, float f){
	return abs(f-p.y);
}
float sph(float3 p,float4 spr){
	return length(spr.xyz-p) - spr.w;
}
float cly(float3 p,float4 cld){
	return length(float2(cld.x+0.5*sin(p.y+p.z*2.0),cld.z)-p.xz) - cld.w;
}
float scene(float3 p){
	float d=flr(p,-5);
	d=min(d,flr(p,5));
	d=min(d,sph(p,float4( 0,-2,15,1.5)));
	d=min(d,sph(p,float4(-8*tm,0,20,2.0)));
	d=min(d,sph(p,float4(-5,4,15,0.5)));
	d=min(d,sph(p,float4(-1,3*sin(10*tm),15,2.0)));
	d=min(d,sph(p,float4( 2,-3,15,.5)));
	d=min(d,cly(p,float4(10,0,20,1.0)));
	d=min(d,cly(p,float4( 4,0,15,1.0)));
	d=min(d,cly(p,float4( 0,0,20,1.0)));
	d=min(d,cly(p,float4(-2,0,25,1.0)));
	d=min(d,cly(p,float4(-6,0,30,1.0)));
	d=min(d,cly(p,float4(-12,0,35,1.0)));
	return d;
}
float3 getN(float3 p){
	float eps=0.01;
	return normalize(float3(
	scene(p+float3(eps,0,0))-scene(p-float3(eps,0,0)),
	scene(p+float3(0,eps,0))-scene(p-float3(0,eps,0)),
	scene(p+float3(0,0,eps))-scene(p-float3(0,0,eps))
	));
}
float AO(float3 p,float3 n){
	float dlt=0.5;
	float oc=0.0,d=1.0;
	for(int i=0;i<6;i++){
		oc+=(i*dlt-scene(p+n*i*dlt))/d;
		d*=2.0;
	}
	
	float tmp = 1.0-oc;
	return tmp;
}

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float3 org : TEXCOORD0;
	float3 dir : TEXCOORD1;
};

float4 ps( VS_OUTPUT In ) : SV_TARGET
{
	float3 org = In.org;
	float3 dir = In.dir;
	float d,g;
	float3 p=org;
	
	for(int i=0;i<64;i++){
		d=scene(p);
		p=p+d*dir;
	}
	if(d>1){
		return float4(0,0,0,1);
	}
	float3 n=getN(p);
	float a=AO(p,n);
	float3 s=float3(0,0,0);
	float3 lp[3],lc[3];
	lp[0]=float3(-4,0,4);
	lp[1]=float3(2,3,8);
	lp[2]=float3(4,-2,24);
	lc[0]=float3(1.0,0.5,0.4);
	lc[1]=float3(0.4,0.5,1.0);
	lc[2]=float3(0.2,1.0,0.5);
	for(int i=0;i<3;i++){
		float3 l,lv;
		lv=lp[i]-p;
		l=normalize(lv);
		g=length(lv);
		g=max(0.0,dot(l,n))/(g)*10;
		s+=g*lc[i];
	}
	float fg=min(1.0,20.0/length(p-org));
	return float4(s*a,1)*fg*fg;
}

VS_OUTPUT vs(uint vid : SV_VertexID )
{
	float4 v[] = {
		{ float4(-1, 1,0,1) },
		{ float4( 1, 1,0,1) },
		{ float4( 1,-1,0,1) },
		{ float4( 1,-1,0,1) },
		{ float4(-1, 1,0,1) },
		{ float4(-1,-1,0,1) },
	};
	VS_OUTPUT Out;
	Out.pos = v[vid];
	Out.org = float3(0,0,0);
	Out.dir = normalize(float3(Out.pos.x*1.333f, Out.pos.y, 2.f));
	return Out; 
}
