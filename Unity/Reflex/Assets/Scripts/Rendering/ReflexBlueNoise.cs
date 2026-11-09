// LIGHTING: ENHANCED > VOLUMETRIC LIGHT (Scripts/Rendering/ReflexVolumetricLight.cs): a 64 x 64 blue-noise tile (void-and-cluster, Ulichney 1993,
// gaussian sigma 1.9, ranks quantised to 8 bits) for the ray march's start offsets. Generated once offline; not part of the original game.
using System;
using UnityEngine;

namespace Reflex.Port.Rendering
{
    static class ReflexBlueNoise
    {
        public const int Size = 64;
        static Texture2D texture;

        /// The tile as an R8 texture (point sampled, repeat).
        public static Texture2D Texture
        {
            get
            {
                if (texture) return texture;
                texture = new Texture2D(Size, Size, TextureFormat.R8, false, true) { name = "Reflex blue noise 64", filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Repeat, hideFlags = HideFlags.DontSave };
                texture.SetPixelData(Convert.FromBase64String(Data), 0);
                texture.Apply(false, true);
                return texture;
            }
        }

        const string Data =
            "43A8Go3nv0Eh35AOZeIC9TPaRJNasGlK7tc68ZMTVpor0Wz7iC3Wgr2r/NoJZn3WEbKGaZwlWqcOMmCPeyihXCCCtV/TdKVm7FKxNv+BKZZrpgeBJtIXg79iJn5F4PjAYRA4q5xE8ZUK" +
            "yVyFHsXvPlr5LRq4CYDcTfga4lP/kkfuMKPyVBI2uIoqzXanQFu/Hudi8MFx9i6nBJ7NaS8Bg7DqfdwGxWQg4UsxouVQjAOX3KlQ5c89bsmynUO8B8ivZtYEySiB9gGeXvEJmBfr0Eh7" +
            "tDmYCkKN5nX/G7qMTtcdlUcnV3SlOH1rEpRxK6+/I245kHb8mB6JA2mANdlyETuXd0WSwdxKfBy+SNa1cIygESviUqreXLdNN1mq6qA8cFvMtecZ97LR6rr1PNBhe0fqxRRer0gw69Un" +
            "8qkeVoi+UemyGVyobsvmM2eGVC8H+VnHiBrMeiENxpfdEXYj88QJ/oQ0j00AiShXqQfeFv+kC4LaKQXAY6JQt12N5Z/6FSTbbTn+LA4/jqr4Id/FPa3ZbkT9ZzSg2W8ohETQZLMymhNp" +
            "ocPdXpxCHcSBTZs0zGezTJ7vheF3FDvGCkgsz4NeowmKzprgulgIoHgS7mV/JJa2BJHwsT7sYfm+ClWI4XpR2EMrDm/K/HhlkOm5WYwg8zLRbUAOyv6Xbtd8ZKz2Nrjse1BlIILtKdJE" +
            "kbueSxbqMVzBTIAXjAKuMOWkGD+8qyHwfbrkNa4Q2S8icwFB4XhUkBu5WYwtrxn0ukECxpBJKMMS8rNMccRirzNYAPfJpHfkDyjVV8qcT3uUbcn4AmPNk1emHoZQlvFGp9H4v5gUrMf6" +
            "JqTbSoBVJZzmcNUbZ+GgQJLVAzmX/RrldYrVOVOG0Wud9nO7H9Y58SRJjy7qchX6P2jtCcxbthdmgSlf1wdlN3XpAWnQ4jiKD1CZ8HgGsFswdqbhEIVP18Ala7ILILpCrBMvRONqDrld" +
            "23eiTji2BYzXLbxyO4ef40yxPO6hSIe0mUK9EqhdxLIvqlg9zvyDyB30V7lsLZ8OQ+iVY/yPNep+mGD7qILNG6wMwuOCm8lNrHudId0F8zMNy4oie+bUF2D4M5Twdh/7fwq/jS0Xat9H" +
            "iiXMQK3tXoCmLcRM3wJa2LUKiipTmUT+hyhaGdolYucTQ/5kxVR3lm39UcEwDVXEI26FB0zWZ0XrI9ubU6sLwGGdffcIjs4b9dkSdaJoxR5Nzz3AA+VzZTvPs2zudzW+WdSBrJEmvdkV" +
            "qQJnk6fxf9uvzT3mlxTLiGFw9bpBlO811BhScTezTXlZPbaEKfWRduxsn9UzteuUBfVDkwmk9I4LMEsY6DizW+o+0bdyOkmfG1O3J6ZzNbWjSAR84yhytAHlwZnfKb0Dm8oc6EaoOA+s" +
            "JPZYfBEhqFF+MK/HSh9rt8nvoml+Rx+ahOAZJ4voA2b/jlzA91Ed5xPTOIYcWKRDaIUdX/uIaPAyjtMHWMDcYYMXQo/JX9zDZxPWVoXiO5h0XQPejPnJcjFMYPrHri583Q0yggDSeC7A" +
            "X6fG9NaO/yuvSdIPp0Pirl9w/3ubLEfMuOKl+ixGnuSN+ymhzRH6J9VAwhAtpgbywJ4JVmzUQ5zEbeJBmrCL+06WC2ZLDnfL6547dMUffgtOJ7QU5IvwDJdoNgOIbww6HrtyAGN9q1OE" +
            "tZZPZeVYj7B32RaWuiDzTKok8GTcO24i6Hgtt8M4GVcGkPEwVdiTzaDDP2pTsHQoT8J51LnzrXtcQe+0R+cVNfQceK/RI0QSOuyATjaJEl/Qkw9UGZ8Gz6xA3aBdldp/vN9itaL4Fzby" +
            "hR/UATf52RztqlcjlU3o0pwz2STBjKFtyOo4m4Hdaswpp/fK4LB5OIS7yn7zXL2DFO6HI/dpqU0mFYdGbbx4SmLpqZLHf6FejkAQ32QuBsQWgpJqWAXgXCoISr8N/rdaixxlAXAs6gr9" +
            "SSu14EaQMVVvAq5CDC/zb8vqACtb5AiZMHFaFka7CMww/oO/popsVP0LqfXPPbh9+I5gcTGhS+bAPZxSvaFa1qdylhAkatj9xUrU5I3GoD96rpnPi6XWE8r9tt4rZ+d6tGxGGfA5s95E" +
            "ySx4Sx+X26Ua0+ggkwZzr9p++RhCjR9lNVLI7aCzHJgrdbpWgR3XDlM6/BpBuH1PIj+G9JpPIKWR2VvPdiSiG2K8nuiHDGdSQayDVcj1FzJeIpDOasHmA/eFPXkFXjuApmEWN/1ktJHh" +
            "YMJyJu1njaEFrnUP0Tz2ASqeDUvnlH/uNxRvsM0y7cMAOrLYQofE8EqrB9w6erDQE6fBTOTTDPXokwTOSCXvMQuD261YMsDx0F02wolh3VXFfvpltwTQUY3YVkD5JHmSbPspemSlCpl2" +
            "5C+GTp0pW9ts+iaJbrZPQMGr3nKGo7xqnUoDld4RR2zkJaDuFbhzrjaNzCw+cKsm9ALFYJ65FtpbnBzkUNE5uxNZt/IO6ZAyG5WwM5vLIX1pGppZEUDNHfM3zvl3IIOUDLRLfC+XRB/k" +
            "FadY8N8Ruph8qBuJReYLyou+D3DsJWP/lm4hxHRFt/FjCulZFN8wivApuffje0+rhxhhtjqp1lb1yWjiCM7xUG6+hx56X0YuauA203NTpTdK8zCqkIDJRNekO2GqzVV+PsdGeP+kXsXU" +
            "UDaOAl4r6W/GoVLrwix0PhmPWKqEYKEG20Waw/+Lz+xPD/yyJ+9qf7XVQVcBsR2KCt/4hQAq4aPXh7sDkLBDCHWoaMex2ZMKQCiPCGP7nQGu1CL+vzwqfPgxDa44B6MhvpNdg8IalwVf" +
            "IZ7h9jR4UrouSxiavm8QH2grUTntGIL63CGcRxS+Vf/T4Ed9HYm76UY0cw3elbTNW2vX53RXgT+tBjHXQ+HL/HcRvGWYzvFpknbT6olN+ZSr8sJz0WSVvA08fvYydaWCaBWwyDfcXWuA" +
            "maZNZ+wgTqmRJkm4F/TLZuigcFaLqDVQiNIpShXkJceoP2CyMs48WtwOnSLkLkpc69OKZOEfuTWacOinTynLGvbYFMeKAHPxFYTIYpXdJHlLFPW7ECdoses9cauEnQRY/Q4h8Ah9uBiA" +
            "RYlWtqTNcaooUgXK7EwN9CZaBpXvDbVVLHusRTO43jyf+jECpTmN0LCUPHzmwxiVBd7CX0K0NH/cn5Fj426l6Mcz/AB8HZAQwbORPp1fiM/BjEPTcYRBw5HpXvzUgGMLVa/VcFG96xwu" +
            "YgHSTZ3zXH4j+xHt2Y7AblI/wh9LLAZfrddpP/TlOGf7G3opst47eRr8rjOg4GYeOgmWJaHG6ngfRIf3DVpt4cb+h3ItQ822T6IveGVKGivOpoH80pvzd5ITTr2bVYLbSKPW8W0AVKRm" +
            "vA9f8gaxd9C+cFcYSS27mOPEKrGcgUWiVKwe2wuNbDnTk7sKqvfiAl0TNYxTuz0g74bSJQjKLHMJV8MYl/Yu10t/xydRi/mkTeOz9pDTawhcF3o+8MEkEjfuZr6j+RPphhtU1ps7d7WU" +
            "23OxF2jdyKNyM7FhqJTjuIkzSq2D4yKR6j2Z2BlELhCFNgN8qj7+jqjYZgaM3Xa3B5JKejOtYsRB9GwiiE7sLUbGB+aEKklcEeH/dxbyQh9oz+d1Pgi2bBWrdF265WmZ2Mlm7CNTyTTq" +
            "Tc0wVvmZ01vmxyZY3QR2KuKyy2EOv2r0VqE49q4ElrlBjU+/Nlyq/ZsPvWDJnFfUMf/KAX6v9BxaRZ/dD7aCIXGhFbNsSCuEPRXyiLtOl6aBBzX/nxzjgCPRlV56zO1qINQCnYTYC3ws" +
            "VR35iUXxDoZMIpE5UiV1j8Euh3RhmgC79ZI6yxypaNqvb50e/NgWXUmSetOtiw+5cUIa2lE1p3/pLstv7EfHj9ylNNEndqW/4m6i79G9pgj7rxXO90njP13dhuwLxf2WATTMQWguyPG+" +
            "5iVSQTRgpv0JwIwm+AvCVWWwGye5YzrvbX4Dr+U7ZAq0QhdjMeNAbU/nOacoxX4RJHOjU30jRlntfwy1j3M7EaNvAMfV6ylO3WagtHJGkqI8+oxTmgWuFkm7WpEZUJoq93vbiZgRgdUh" +
            "llsJbY2x1Uy/MeBiuI3BpdZL56lVhdpjufiXGXWRyIMx6BJb1eMYeQzcznT2gsqe6Ndp+sjei8JOBcdXtetgxXq62u1SNPxnlAT1PRDlLXQSYifPBB+xRS6BWkizBTupHXk+yIYpt+5J" +
            "py9A4SJWLQ0/Ibd/EF01qWso+Ug1qwItRYUcmwymG9Kuh53Paxz6mYfyNp/7kukT4J/A9GPiVvK9mPsEN21ZxH9hEr1olP55i6kyR3LVG+6f33Udm4f+0qLzYsx04lY6cCJJs4FUOce2" +
            "W3u+alB1xTwk2H0Wm9BrDktirZ/RIpcAs/KIRqzVwGDunAH1lrFAhA+80GYUTnENO7gnQryC7sNb2gzsqd9HFwnbQSrRCYqta1IviUMosowu333pQ4r93Tmfzxs0Bk0T0FThviNRzmAv" +
            "kD/vsuKUWK19548CmiwU/nkylGQli2+r6pUZ76Nc/hDppLr6CHXrxB9TCrwWZ1Enc1nrbZnfdCiFZDqPeucE9lOmeyc3vyDbElH6atdLpI+5QssAvffUMVSFY7RIMcyWA3HIXtxQojzX" +
            "knH0MqvIkuUPv4HzPK65GqPMaxa3o3HC2Qteyor4ZaDGNB2yYOYHax3yo057niDLvwLgeh6CP7VKIDaTGINnEqjOXITZCHxJsSykIFqO+EXpCf8yR98iORmF60kEdSxFhqp4yj2A0FTg" +
            "hF42FGdD/HM4m/XB1mPh9XzTre+9/jC3SCSZQLod92VBytgEacYweVCsxJR/Y5z8uWqqnea21AfrWQ30IjSwmSrC2O22khCoJldrEakqhhafZStGAVl83wzodvFYot6LDJV5TOUXmNok" +
            "iVvzDNRRjkIq0Bs6VJNtKNmOvKFw+UgYdK0FheVM3tCOQuxQk75WB+iLb82bP45rpRjKKm400VT7szOngFa0b+QUPiuoyADieVnygA7A/z+cTWQTiMUJ6Y8+V8YrX3y6GbF3Cjdvz7I6" +
            "xh7ktCX3w1E4s5EFgL0WbCTvwQ3TOwKezWq47HE0shKYw2jcpR59sTDw20BY0mai/iFumjoM+THayKD9HETuXaR+TxLSYgSJ3V/7S+esPuCeXkSR+mTtvH5OmBddg/baTCOMMkhg5RjC" +
            "AXqsI7h/L0zZtPDLo2mIWSJi3i+XeSYM+5M3dqsv7nsU0qQlYY/EA4ZyHiymRxoz/YjjRyaiZjyt+AO2zIlX0m2Q5jiW4BWqeQgcglHtAJpIf6yKUtjDabDaWvOcRrwjmT1yEO94L0/W" +
            "57fLdoveXyPUB7XGkArPd+Sebu0QmDb3Thlh8wVVzo1kROAmrT7muPIGvQ/4nUktBoPGFm/VT67GiFO2zBr9oTcPl1QLr8NwqD547lAeu10VUjwndkWouSqgyXVDwus19r6VdMPSE2w3" +
            "zg==";
    }
}
