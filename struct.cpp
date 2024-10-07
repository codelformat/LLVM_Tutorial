Exp {
    type = ExpType::LIST,
    list = {
        Exp { type = ExpType::SYMBOL, string = "begin" },
        Exp { type = ExpType::LIST, list = {
            Exp { type = ExpType::SYMBOL, string = "var" },
            Exp { type = ExpType::SYMBOL, string = "VERSION" },
            Exp { type = ExpType::NUMBER, number = 45 }
        }},
        Exp { type = ExpType::LIST, list = {
            Exp { type = ExpType::SYMBOL, string = "printf" },
            Exp { type = ExpType::STRING, string = "Version: %d\n" },
            Exp { type = ExpType::SYMBOL, string = "VERSION" }
        }}
    }
}