# SiNUCA3

Terceira iteração do Simulator of Non-Uniform Cache Architectures (Simulador de
Arquiteturas de Cache Não-Uniformes).

## Principais autores

O SiNUCA3 é desenvolvido pelos seguintes alunos do 
[Dr. Marco Zanata](https://github.com/mazalves):

- [Gabriel G. de Brito](https://github.com/gboncoffee)
- [Vinícius Santos](https://github.com/Vini72SH)
- [Fernando de Barros Castro](https://github.com/ferbcastro)
- [Bruno Krügel](https://github.com/bk304)
- [Daniel Siqueira](https://github.com/Dalien-S)
- [Pedro Endrigo dos Santos](https://github.com/Endrigopedro)
- [Giuliano Tavares](https://github.com/GiuTP)

## Dependências

O SiNUCA3 depende da libyaml (`sudo apt install libyaml-dev` no Debian, Ubuntu e
derivados).

## Compilando

Só `make` funciona.

Nós usamos um Makefile que automaticamente detecta todos os arquivos-fonte em
`src/`, compila cada um para seu próprio objeto em `build/` e liga tudo como
`sinuca3`. Além disso, ele usará o `clang` caso você o tenha instalado, mas
tentará o `gcc` ou `cc` caso contrário.

Dito isso, você pode compilar um binário otimizado simplesmente rodando `make`,
e um binário para debug rodando `make debug`. Ambos podem coexistir já que o
nome dos objetos e dos binários é diferente (os de debug possuem o sufixo
-debug).

Caso você mexa em algum header, infelizmente terá que rodar `make -B` pois o
sistema de build não recompila automaticamente arquivos-fonte quando seus
headers foram modificados.

## Estrutura do projeto

Como dito na seção de compilação, você pode simplesmente jogar um arquivo .cpp
ou .c dentro de `src/` e ele será automaticamente detectado pelo sistema de
build. Para criar seus próprios componentes, pode ser uma boa ideia manter tudo
organizado e colocá-los no diretório `src/custom/`.

Você também deverá adicionar seus componentes ao arquivo `config.cpp`.

A maior parte da API é acessível pelo header `sinuca3.hpp`.

## Esquema de modularização

Componentes de simulação são definidos aproximadamente como estágios de um
pipeline. Eles se comunicam por uma interface de mensagens. A cada ciclo, um
componente por ler mensagens transmitidas pelos outros no ciclo anterior. Para
criar um componente, basta herdar de `Component<T>`, onde T é o tipo de mensagem
que ele recebe.

Para simular um sistema, defina-o em arquivos de configuração yaml.

Quando precisar reusar código, prefira composição, isso é, adicione a classe
específica como um atributo privado e chame seus métodos.

## Desenvolvimento, guia de estilo, etc

Nós gostamos de usar ferramentas modernas como `clangd` e `lldb` e padrões
antigos: nosso C++ é C++98 e nosso C é C99. Quando adicionar novos arquivos,
seja gentil e rode `bear --append -- make debug` para que o `clangd` detecte-os.
Depois de editar, formate tudo com o `clang-format`. Um truque comum é rodar
`find . -type f -name "*.[cpp,hpp,c,h]" -exec clang-format -i {} \;` pois isso
formatará todos os arquivos.

Além disso, o projeto é documentado com o Doxygen, então lembre dos comentários
de documentação.

Nós seguimos a maior parte do [Guia de Estilo C++ do
Google](https://google.github.io/styleguide/cppguide.html) com algumas
extensões:

- Quanto ao estilo de código, usamos o seguinte `.clang-format`:  
  ```
  BasedOnStyle: Google
  IndentWidth: 4
  AccessModifierOffset: -2
  PointerAlignment: Left
  ```
- Usamos `.cpp` para arquivos C++ e `.hpp` para headers C++. Igualmente, os
  "include guards" terminam com `HPP_`.
- `static` ainda deve ser evitado porém somos mais permissivos que o Google por
  conta da natureza do nosso projeto.
- Geralmente, deve-se somente herdar de classes virtuais.
- "Operator overloading" é completamente proibido.
- Não utilizamos referências, somente pointeiros. Claro que não utilizamos
  "smart pointers" e "move semantics" pois estamos presos no C++98.
- Não temos o `cpplint.py`.
- Não use classes amigas.
- Use `cstdio` ao invés de `iostream`.
- Use `NULL` para ponteiros nulos pois estamos presos no C++98.
- Não use dedução de tipos.
- Nenhuma biblioteca boost é aprovada.
- Variáveis, atributos e namespaces são `camelCase` e sem nenhum prefixo ou
  sufixo.
- Constantes são `ALL_CAPS`, assim como macros (que claro devem ser evitados).
- Nomes em enums são `PascalCase` assim como funções e tipos.
- No geral, evite complicar as coisas.

O HiPES é um espaço inclusivo. Lembre-se disso.
