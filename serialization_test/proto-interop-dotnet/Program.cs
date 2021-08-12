using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using ProtoBuf;

namespace proto_interop_dotnet
{
    [ProtoContract]
    class InnerTestStruct {
        [ProtoMember(1,DataFormat = DataFormat.ZigZag)]
        public Int32 a {get; set;}
        [ProtoMember(2,DataFormat = DataFormat.FixedSize)]
        public Int64 a1 {get; set;}
        [ProtoMember(3)]
        public Int32 a2 {get; set;}
        [ProtoMember(4)]
        public double b {get; set;}
        [ProtoMember(1001)]
        public List<string> c {get; set;}
        [ProtoMember(1002)]
        public string d {get; set;}
        public override string ToString()
        {
            var x = new StringBuilder();
            x.Append($"a={a},a1={a1},a2={a2},b={b},c=[");
            foreach (var y in c) {
                x.Append(y).Append(' ');
            }
            x.Append($"] d={d}}}");
            return x.ToString();
        }
    }
    [ProtoContract]
    class OuterTestStruct {
        [ProtoMember(1)]
        public List<float> f {get; set;}
        [ProtoMember(2)]
        public InnerTestStruct g {get; set;}
        [ProtoMember(3)]
        public bool h {get; set;}
        public override string ToString()
        {
            var b = new StringBuilder();
            b.Append("{f=[");
            foreach (var x in f) {
                b.Append(x);
                b.Append(' ');
            }
            b.Append("],g={").Append(g.ToString()).Append("},h=").Append(h).Append("}");
            return b.ToString();
        }
    }
    class Program
    {
        static void Main(string[] args)
        {
            byte[] data = new byte[] {
                0x0a, 0x0c, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x40, 0x40, 0x12, 0x34, 0x08, 0x49, 0x11, 0xce, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x18, 0xba, 0xff, 0xff, 0xff, 0x0f, 0x21, 0xe1, 0x7a, 0x14, 0xae, 0x47, 0x81, 0x24, 0x40, 0xca, 0x3e, 0x05, 0x61, 0x62, 0x63, 0x64, 0x65, 0xca, 0x3e, 0x03, 0x62, 0x63, 0x64, 0xca, 0x3e, 0x03, 0x63, 0x64, 0x65, 0xd2, 0x3e, 0x03, 0x78, 0x79, 0x7a
            };
            var s = Serializer.Deserialize<OuterTestStruct>(new MemoryStream(data));
            Console.WriteLine(s);
        }
    }
}
