# Avisos de terceiros

O Animator-R-F usa componentes de código aberto. Todos permitem uso comercial.

## libmypaint (motor de pincéis)
- Origem: https://github.com/mypaint/libmypaint (versão 1.6.1)
- Licença: ISC (permite usar, copiar, modificar e distribuir, mantendo o aviso de copyright)
- Copyright (C) 2007-2012 Martin Renold e (C) 2012-2016 a equipe de desenvolvimento do MyPaint.

## mypaint-brushes (pincéis prontos)
- Origem: https://github.com/mypaint/mypaint-brushes (versão 1.3.1)
- Licença: CC0 1.0 (domínio público). A política do projeto aceita apenas pincéis em domínio público/CC0.
- Os pincéis são obra de vários artistas da comunidade MyPaint (por exemplo, os conjuntos
  classic, deevad, experimental, kaerhon_v1, ramon e tanda).

## json-c (leitura dos arquivos .myb)
- Origem: https://github.com/json-c/json-c (versão 0.17)
- Licença: MIT.

## minih264 (codificador de vídeo H.264) e minimp4 (gravador de MP4)
- Origem: https://github.com/lieff/minih264 e https://github.com/lieff/minimp4
- Licença: CC0 1.0 (domínio público).
- Observação: a CC0 cobre os direitos autorais do código. O formato H.264 em si tem patentes
  licenciadas pelo grupo MPEG LA/Via LA; para distribuir comercialmente em grande escala,
  consulte as regras de licenciamento do H.264 (hoje há isenção para volumes pequenos).

Todos são baixados no momento da configuração do build (`third_party/CMakeLists.txt`) e não são
copiados para este repositório.
