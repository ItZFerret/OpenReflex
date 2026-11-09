// RxJson: a small JSON value type (J), ported from Open Unleashed's MxaJson (same author) with JavaScript's semantics: a missing key reads
// as Undefined (not null), Number.isInteger, typeof checks, truthiness, and JSON.stringify's output (integral numbers without a decimal
// point, NaN / Infinity as null, undefined members left out). Objects keep insertion order. Parse never throws anything but
// FormatException / OverflowException (TryParse catches both), and nesting is capped at MaxDepth. Used by the mod readers (.rxmod mod.json).
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace Reflex.Port.Json
{
    public enum JKind {Undefined,Null,Bool,Number,String,Array,Object}
    public sealed class J : IEnumerable<J>
    {
        public static readonly J Undefined=new J(JKind.Undefined),Null=new J(JKind.Null),True=new J(true),False=new J(false);
        public const int MaxDepth=32;
        public readonly JKind Kind;
        readonly double num;readonly string str;readonly bool flag;
        readonly List<J> items;readonly List<string> keys;readonly Dictionary<string,J> map;

        J(JKind k){Kind=k;if(k==JKind.Array)items=new List<J>();else if(k==JKind.Object){keys=new List<string>();map=new Dictionary<string,J>(StringComparer.Ordinal);}}
        J(bool b){Kind=JKind.Bool;flag=b;}
        J(double d){Kind=JKind.Number;num=d;}
        J(string s){Kind=JKind.String;str=s;}
        public static J Obj()=>new J(JKind.Object);
        public static J Arr()=>new J(JKind.Array);
        public static J Num(double d)=>new J(d);
        public static J Str(string s)=>s==null?Null:new J(s);
        public static J Bool(bool b)=>b?True:False;
        /// An object from key / value pairs: J.O("name",name,"bytes",n). Values go through From.
        public static J O(params object[] kv){var o=Obj();for(int i=0;i+1<kv.Length;i+=2)o.Set((string)kv[i],From(kv[i+1]));return o;}
        public static J A(params object[] values){var a=Arr();foreach(var v in values)a.Add(From(v));return a;}
        public static J From(object v)
        {
            switch(v)
            {
                case null:return Null;
                case J j:return j;
                case string s:return new J(s);
                case bool b:return b?True:False;
                case int i:return new J(i);
                case long l:return new J(l);
                case uint u:return new J(u);
                case double d:return new J(d);
                case float f:return new J((double)f);
                case byte by:return new J(by);
                case IDictionary<string,J> dict:{var o=Obj();foreach(var kv in dict)o.Set(kv.Key,kv.Value);return o;}
                case IEnumerable e:{var a=Arr();foreach(var x in e)a.Add(From(x));return a;}
                default:throw new ArgumentException("No JSON form for "+v.GetType().Name);
            }
        }
        public static implicit operator J(string s)=>Str(s);
        public static implicit operator J(double d)=>new J(d);
        public static implicit operator J(int i)=>new J(i);
        public static implicit operator J(long l)=>new J(l);
        public static implicit operator J(bool b)=>b?True:False;

        // ---- reading (JavaScript-like)
        public bool IsUndefined=>Kind==JKind.Undefined;
        public bool IsNullish=>Kind==JKind.Undefined||Kind==JKind.Null;
        public bool IsString=>Kind==JKind.String;
        public bool IsNumber=>Kind==JKind.Number;
        public bool IsBool=>Kind==JKind.Bool;
        public bool IsArray=>Kind==JKind.Array;
        public bool IsObject=>Kind==JKind.Object;
        /// Number.isInteger
        public bool IsInteger=>Kind==JKind.Number&&!double.IsNaN(num)&&!double.IsInfinity(num)&&Math.Floor(num)==num;
        public bool IsFinite=>Kind==JKind.Number&&!double.IsNaN(num)&&!double.IsInfinity(num);
        public double Number=>Kind==JKind.Number?num:double.NaN;
        public int Int=>Kind==JKind.Number?(int)num:0;
        public long Long=>Kind==JKind.Number?(long)num:0;
        public string String=>Kind==JKind.String?str:null;
        public bool Boolean=>Kind==JKind.Bool&&flag;
        /// JavaScript truthiness
        public bool Truthy=>Kind switch{JKind.Bool=>flag,JKind.Number=>num!=0&&!double.IsNaN(num),JKind.String=>str.Length>0,JKind.Array=>true,JKind.Object=>true,_=>false};
        public int Count=>Kind==JKind.Array?items.Count:Kind==JKind.Object?keys.Count:0;
        public J this[string key]{get=>Kind==JKind.Object&&map.TryGetValue(key,out var v)?v:Undefined;set=>Set(key,value);}
        public J this[int i]=>Kind==JKind.Array&&i>=0&&i<items.Count?items[i]:Undefined;
        public bool Has(string key)=>Kind==JKind.Object&&map.ContainsKey(key);
        public IReadOnlyList<string> Keys=>Kind==JKind.Object?keys:(IReadOnlyList<string>)Array.Empty<string>();
        public List<J> Items=>items;
        public IEnumerator<J> GetEnumerator(){if(items!=null)foreach(var x in items)yield return x;}
        IEnumerator IEnumerable.GetEnumerator()=>GetEnumerator();

        // ---- building
        public J Set(string key,J value)
        {
            if(Kind!=JKind.Object)throw new InvalidOperationException("not an object");
            value??=Null;if(!map.ContainsKey(key))keys.Add(key);map[key]=value;return this;
        }
        public J Set(string key,object value)=>Set(key,From(value));
        public bool Remove(string key){if(Kind!=JKind.Object||!map.Remove(key))return false;keys.Remove(key);return true;}
        public J Add(J value){if(Kind!=JKind.Array)throw new InvalidOperationException("not an array");items.Add(value??Null);return this;}
        /// A shallow copy (the spread {...o}), optionally with more members.
        public J Copy(params object[] kv)
        {
            J c;if(Kind==JKind.Object){c=Obj();foreach(var k in keys)c.Set(k,map[k]);}else if(Kind==JKind.Array){c=Arr();foreach(var x in items)c.Add(x);}else return this;
            for(int i=0;i+1<kv.Length;i+=2)c.Set((string)kv[i],From(kv[i+1]));return c;
        }
        public J DeepCopy(){if(Kind==JKind.Object){var c=Obj();foreach(var k in keys)c.Set(k,map[k].DeepCopy());return c;}if(Kind==JKind.Array){var c=Arr();foreach(var x in items)c.Add(x.DeepCopy());return c;}return this;}

        // ---- JSON text
        public override string ToString()=>Stringify(this);
        public static string Stringify(J v,bool pretty=false){var sb=new StringBuilder();Write(sb,v,pretty?"  ":null,"");return sb.ToString();}
        public static string NumberText(double d)
        {
            if(double.IsNaN(d)||double.IsInfinity(d))return "null";
            if(Math.Floor(d)==d&&Math.Abs(d)<1e21)return d==0?"0":d.ToString("0",CultureInfo.InvariantCulture);
            string r=d.ToString("R",CultureInfo.InvariantCulture);
            // JavaScript's exponent form: 1e-7, 1.5e+21
            int e=r.IndexOf('E');if(e>=0){string m=r.Substring(0,e),x=r.Substring(e+1);if(x[0]!='-'&&x[0]!='+')x="+"+x;x=x[0]+x.Substring(1).TrimStart('0');r=m+"e"+x;}
            return r;
        }
        static void Write(StringBuilder sb,J v,string indent,string at)
        {
            switch(v.Kind)
            {
                case JKind.Undefined:case JKind.Null:sb.Append("null");return;
                case JKind.Bool:sb.Append(v.flag?"true":"false");return;
                case JKind.Number:sb.Append(NumberText(v.num));return;
                case JKind.String:Quote(sb,v.str);return;
                case JKind.Array:
                {
                    if(v.items.Count==0){sb.Append("[]");return;}
                    string inner=indent!=null?at+indent:null;sb.Append('[');
                    for(int i=0;i<v.items.Count;i++){if(i>0)sb.Append(',');if(inner!=null)sb.Append('\n').Append(inner);var x=v.items[i];Write(sb,x.IsUndefined?Null:x,indent,inner);}
                    if(inner!=null)sb.Append('\n').Append(at);sb.Append(']');return;
                }
                default:
                {
                    string inner=indent!=null?at+indent:null;bool any=false;sb.Append('{');
                    foreach(var k in v.keys)
                    {
                        var x=v.map[k];if(x.IsUndefined)continue;if(any)sb.Append(',');any=true;if(inner!=null)sb.Append('\n').Append(inner);
                        Quote(sb,k);sb.Append(indent!=null?": ":":");Write(sb,x,indent,inner);
                    }
                    if(any&&inner!=null)sb.Append('\n').Append(at);sb.Append('}');return;
                }
            }
        }
        static void Quote(StringBuilder sb,string s)
        {
            sb.Append('"');
            foreach(char c in s)
            {
                switch(c)
                {
                    case '"':sb.Append("\\\"");break;case '\\':sb.Append("\\\\");break;
                    case '\n':sb.Append("\\n");break;case '\r':sb.Append("\\r");break;case '\t':sb.Append("\\t");break;
                    case '\b':sb.Append("\\b");break;case '\f':sb.Append("\\f");break;
                    default:if(c<0x20)sb.Append("\\u").Append(((int)c).ToString("x4"));else sb.Append(c);break;
                }
            }
            sb.Append('"');
        }

        /// JSON.parse; throws FormatException on bad input.
        public static J Parse(string text)
        {
            int i=0;var v=ParseValue(text,ref i,0);SkipWs(text,ref i);if(i!=text.Length)throw new FormatException("Unexpected text after JSON");return v;
        }
        public static bool TryParse(string text,out J value){try{value=Parse(text);return true;}catch(Exception e) when(e is FormatException||e is OverflowException){value=Undefined;return false;}}
        static void SkipWs(string s,ref int i){while(i<s.Length&&(s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r'))i++;}
        static J ParseValue(string s,ref int i,int depth)
        {
            if(depth>MaxDepth)throw new FormatException("JSON too deep");
            SkipWs(s,ref i);if(i>=s.Length)throw new FormatException("Unexpected end of JSON");
            char c=s[i];
            if(c=='{')
            {
                i++;var o=Obj();SkipWs(s,ref i);if(i<s.Length&&s[i]=='}'){i++;return o;}
                while(true)
                {
                    SkipWs(s,ref i);if(i>=s.Length||s[i]!='"')throw new FormatException("Expected a key");
                    string k=ParseString(s,ref i);SkipWs(s,ref i);if(i>=s.Length||s[i]!=':')throw new FormatException("Expected :");i++;
                    o.Set(k,ParseValue(s,ref i,depth+1));SkipWs(s,ref i);
                    if(i<s.Length&&s[i]==','){i++;continue;}if(i<s.Length&&s[i]=='}'){i++;return o;}throw new FormatException("Expected , or }");
                }
            }
            if(c=='[')
            {
                i++;var a=Arr();SkipWs(s,ref i);if(i<s.Length&&s[i]==']'){i++;return a;}
                while(true)
                {
                    a.Add(ParseValue(s,ref i,depth+1));SkipWs(s,ref i);
                    if(i<s.Length&&s[i]==','){i++;continue;}if(i<s.Length&&s[i]==']'){i++;return a;}throw new FormatException("Expected , or ]");
                }
            }
            if(c=='"')return new J(ParseString(s,ref i));
            if(Match(s,ref i,"true"))return True;if(Match(s,ref i,"false"))return False;if(Match(s,ref i,"null"))return Null;
            int start=i;if(i<s.Length&&s[i]=='-')i++;
            if(i>=s.Length||!char.IsDigit(s[i]))throw new FormatException("Unexpected character");
            if(s[i]=='0')i++;else while(i<s.Length&&char.IsDigit(s[i]))i++;
            if(i<s.Length&&s[i]=='.'){i++;if(i>=s.Length||!char.IsDigit(s[i]))throw new FormatException("Bad number");while(i<s.Length&&char.IsDigit(s[i]))i++;}
            if(i<s.Length&&(s[i]=='e'||s[i]=='E')){i++;if(i<s.Length&&(s[i]=='+'||s[i]=='-'))i++;if(i>=s.Length||!char.IsDigit(s[i]))throw new FormatException("Bad number");while(i<s.Length&&char.IsDigit(s[i]))i++;}
            return new J(double.Parse(s.Substring(start,i-start),NumberStyles.Float,CultureInfo.InvariantCulture));
        }
        static bool Match(string s,ref int i,string word){if(string.CompareOrdinal(s,i,word,0,word.Length)!=0)return false;i+=word.Length;return true;}
        static string ParseString(string s,ref int i)
        {
            i++;var sb=new StringBuilder();
            while(true)
            {
                if(i>=s.Length)throw new FormatException("Unterminated string");char c=s[i++];
                if(c=='"')return sb.ToString();
                if(c<0x20)throw new FormatException("Control character in string");
                if(c!='\\'){sb.Append(c);continue;}
                if(i>=s.Length)throw new FormatException("Bad escape");char e=s[i++];
                switch(e)
                {
                    case '"':sb.Append('"');break;case '\\':sb.Append('\\');break;case '/':sb.Append('/');break;
                    case 'b':sb.Append('\b');break;case 'f':sb.Append('\f');break;case 'n':sb.Append('\n');break;case 'r':sb.Append('\r');break;case 't':sb.Append('\t');break;
                    case 'u':if(i+4>s.Length)throw new FormatException("Bad escape");sb.Append((char)int.Parse(s.Substring(i,4),NumberStyles.HexNumber,CultureInfo.InvariantCulture));i+=4;break;
                    default:throw new FormatException("Bad escape");
                }
            }
        }
    }
}
