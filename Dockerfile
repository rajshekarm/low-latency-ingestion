FROM gcc:14

WORKDIR /app

COPY order_generator.cpp ingestion.cpp ./

RUN g++ -std=c++20 -O2 -o order_generator order_generator.cpp \
    && g++ -std=c++20 -O2 -o cpp_ingestion_client ingestion.cpp

