#include "core/conversation.h"
#include <stdexcept>
#include <utility>


Conversation::Conversation(){}

std::size_t Conversation::size() const noexcept {return size_;}

const Message* Conversation:: begin() const noexcept {return data_; }
const Message* Conversation:: end() const noexcept { return (data_ + size_);}

Conversation:: ~Conversation(){delete[] data_;}

Conversation::Conversation(Conversation&& old) noexcept{
        data_ = old.data_;
        size_ = old.size_;
        capacity_ = old.capacity_;

        old.data_ = nullptr;
        old.size_ = 0;
        old.capacity_ = 0;
}

Conversation& Conversation:: operator=(Conversation&& right) noexcept{
    if ( this == &right){
        return *this;
    }
    delete[] data_;
    data_ = right.data_;
    size_ = right.size_;
    capacity_ = right.capacity_;

    right.data_ = nullptr;
    right.size_ = 0;
    right.capacity_ = 0;  
    
    return *this;
}

Conversation:: Conversation(const Conversation& ref){
    size_ = ref.size_;
    capacity_= ref.capacity_;
    if ( capacity_ > 0){
        data_ = new Message[capacity_];
        for (std:: size_t i = 0; i < size_; i++){
            data_[i] = ref.data_[i];
        }
    }

}

Conversation& Conversation:: operator=(const Conversation& right){
    if ( this == &right){
        return *this;
    }
    Message* left_data = nullptr;
    if (right.capacity_ > 0){
        left_data = new Message[right.capacity_];
        for (std:: size_t i = 0; i < right.size_; i++){
            left_data[i]= right.data_[i];
        }
    }
    delete[] data_;
    data_ = left_data;
    size_ = right.size_;
    capacity_ = right.capacity_;  
    return *this; 
}

const Message& Conversation:: at(std::size_t i) const {
    if ( i >= size_){
        throw std::out_of_range("index is too large");
    }
     return (data_[i]);
    
    }
void Conversation:: append(Message m) {
    std:: size_t new_capcity;

    if (size_ == capacity_){
        if (capacity_ == 0){
            new_capcity = 1;
        }
        else{
            new_capcity = capacity_*2;
        }
        Message* bigger_data = new Message[new_capcity];
        for ( std::size_t i= 0; i < size_; i++){
            bigger_data[i] = std:: move(data_[i]);
        }

        delete[] data_;
        data_ = bigger_data;
        capacity_ = new_capcity;
    }
    data_[size_] = std::move(m);
    size_++;
}