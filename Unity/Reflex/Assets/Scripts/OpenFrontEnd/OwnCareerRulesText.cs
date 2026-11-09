using System;
using System.Collections.Generic;
using Reflex.Port.Content;
namespace Reflex.Port.OpenFrontEnd {
 // Exact RulesPopup.lua RulesType -> authored localization keys. No legacy renderer.
 public sealed class OwnCareerRulesText {
  public readonly string Title,Format,Description;
  OwnCareerRulesText(string title,string format,string description){Title=title;Format=format;Description=description;}
  static readonly Dictionary<string,(string title,string format,string description)> keys=new Dictionary<string,(string,string,string)>(StringComparer.Ordinal){
   ["Freestyle"]=("STR_RULE_TITLE_FREESTYLE","STR_RULE_FORMAT_FREESTYLE","STR_RULE_DESCRIPTION_FREESTYLE"),
   ["Freeride"]=("STR_RULE_TITLE_FREERIDE",null,"STR_RULE_DESCRIPTION_FREERIDE"),
   ["SectionRace"]=("STR_RULE_TITLE_SECTIONRACE",null,"STR_RULE_DESCRIPTION_SECTIONRACE"),
   ["Faceoff"]=("STR_RULE_TITLE_FACEOFF",null,"STR_RULE_DESCRIPTION_FACEOFF"),
   ["Eliminator"]=("STR_RULE_TITLE_ELIMINATION",null,"STR_RULE_DESCRIPTION_ELIMINATION"),
   ["Overtake"]=("STR_RULE_TITLE_OVERTHROW",null,"STR_RULE_DESCRIPTION_OVERTHROW"),
   ["SpeednStyle"]=("STR_RULE_TITLE_SPEEDSTYLE",null,"STR_RULE_DESCRIPTION_SPEEDSTYLE"),
   ["Outrun"]=("STR_RULE_TITLE_OUTRUNNER",null,"STR_RULE_DESCRIPTION_OUTRUNNER"),
   ["Rival"]=("STR_RULE_TITLE_RIVAL",null,"STR_RULE_DESCRIPTION_RIVAL"),
   ["MotoPractice"]=("STR_RULE_TITLE_MOTO_QUALIFIER","STR_RULE_FORMAT_MOTO_QUALIFIER","STR_RULE_DESCRIPTION_MOTO_QUALIFIER"),
   ["HeatPractice"]=("STR_RULE_TITLE_HEAT_QUALIFIER","STR_RULE_FORMAT_HEAT_QUALIFIER","STR_RULE_DESCRIPTION_HEAT_QUALIFIER"),
   ["Heat"]=("STR_RULE_TITLE_HEAT","STR_RULE_FORMAT_HEAT","STR_RULE_DESCRIPTION_HEAT"),
   ["LCQ"]=("STR_RULE_TITLE_HEAT_LCQ","STR_RULE_FORMAT_HEAT_LCQ","STR_RULE_DESCRIPTION_HEAT_LCQ"),
   ["HeatMainEvent"]=("STR_RULE_TITLE_HEAT_MAIN","STR_RULE_FORMAT_HEAT_MAIN","STR_RULE_DESCRIPTION_HEAT_MAIN"),
   ["Moto"]=("STR_RULE_TITLE_MOTO","STR_RULE_FORMAT_MOTO","STR_RULE_DESCRIPTION_MOTO")
  };
  public static OwnCareerRulesText Read(string rulesType){
   if(!keys.TryGetValue(rulesType??"",out var tuple))throw new NotSupportedException("Unsupported authored RulesType: "+rulesType);
   var data=ReflexUiData.Shared;var ingame=data.Strings("InGame");var shared=data.Strings("Shared");
   string Get(string key){if(key==null)return null;string value;if((ingame.TryGetValue(key,out value)||shared.TryGetValue(key,out value))&&!string.IsNullOrWhiteSpace(value))return value;throw new InvalidOperationException("Missing authored rules text: "+key);}
   return new OwnCareerRulesText(Get(tuple.title),Get(tuple.format),Get(tuple.description));
  }
 }
}
