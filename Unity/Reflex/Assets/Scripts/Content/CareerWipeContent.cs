using System;
using System.Globalization;
using System.Linq;
using Reflex.Port.OpenFrontEnd;
namespace Reflex.Port.Content {
 // Uses the user's actual UI resource. No extracted animation or substitute duration is shipped.
 public static class CareerWipeContent {
  public static CareerWipeAnimation Load(){
   var bytes=ReflexInstall.Packages.Read("MXUI","Shared.ENG","PageWipe.lyr","uicmpnt");
   if(bytes==null||bytes.Length<36||BitConverter.ToUInt32(bytes,0)!=0x4c4d5842||BitConverter.ToUInt32(bytes,4)!=0x103ea)throw new InvalidOperationException("Unsupported PageWipe UI resource.");
   uint packed=BitConverter.ToUInt32(bytes,32);if(packed<6||packed>32*1024*1024||36L+packed>bytes.Length)throw new InvalidOperationException("Invalid PageWipe BXML payload.");
   // uicmpnt includes a binary trailer after its BXML document. Only that declared
   // complete document feeds the existing strict BXML parser/checksum.
   var xml=new byte[checked(36+(int)packed)];Buffer.BlockCopy(bytes,0,xml,0,xml.Length);var root=Rfx.Bxml(xml);
   var background=root.children.Single(n=>n.tag=="UIComponent"&&(n.Child("Name")?.value as string)=="Background");
   // Actual authored VisibleColor is BXML kind1 text, not a vector value.
   var color=background.Child("VisibleColor")?.value as string;
   var parts=color?.Split(',');var rgba=new byte[4];
   if(parts==null||parts.Length!=4)throw new InvalidOperationException("Unsupported PageWipe background color.");
   for(int i=0;i<4;i++)if(!byte.TryParse(parts[i].Trim(),NumberStyles.None,CultureInfo.InvariantCulture,out rgba[i]))throw new InvalidOperationException("Invalid PageWipe color component.");
   uint argb=(uint)rgba[3]<<24|(uint)rgba[0]<<16|(uint)rgba[1]<<8|(uint)rgba[2];
   CareerWipeCurve Read(string name){
    var animation=root.children.Single(n=>n.tag=="CompositeAnimation"&&n.S("name")==name);
    double length=Convert.ToDouble(animation.attributes["length"],CultureInfo.InvariantCulture);
    var component=animation.children.Single(n=>n.tag=="ComponentAnimation"&&n.S("targetComponent")=="Background");
    var keys=component.children.Where(n=>n.tag=="KeyFrame").ToArray();
    var times=keys.Select(n=>Convert.ToDouble(n.attributes["keyFrameIndex"],CultureInfo.InvariantCulture)).ToArray();
    var values=keys.Select(n=>Convert.ToSingle(n.children.Single(c=>c.tag=="Channel"&&c.S("type")=="color_a").value,CultureInfo.InvariantCulture)).ToArray();
    return new CareerWipeCurve(length,times,values);
   }
   return new CareerWipeAnimation(Read("In"),Read("Out"),argb);
  }
 }
}
