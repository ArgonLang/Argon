// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_STOREBUFFER_H_
#define ARGON_LANG_SCANNER_STOREBUFFER_H_

namespace argon::lang::scanner {

    class StoreBuffer {
        unsigned char *buffer_ = nullptr;
        unsigned char *cursor_ = nullptr;
        unsigned char *end_ = nullptr;

        bool Enlarge(size_t increase);

    public:
        ~StoreBuffer();

        bool PutChar(unsigned char chr);

        bool PutCharRepeat(unsigned char chr, int n);

        bool PutString(const unsigned char *str, size_t length);

        [[nodiscard]] size_t GetLength() const {
            if (this->buffer_ == nullptr)
                return 0;

            return (size_t) (this->cursor_ - this->buffer_);
        }

        unsigned int GetBuffer(unsigned char **buffer);
    };

} // namespace argon::lang::scanner


#endif // !ARGON_LANG_SCANNER_STOREBUFFER_H_
