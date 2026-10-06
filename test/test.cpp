#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <memory>


/*
	---------- Products: all products -------------
*/

class Product {

public:
	virtual ~Product() = default;

	virtual uint32_t get_price() const = 0;
	virtual std::string get_description() const = 0;
};

class Book : public Product {
private:
	std::string title;
	uint32_t pages;
	uint32_t price;

public:
	Book(std::string t, uint32_t p, uint32_t pr) : title(std::move(t)), pages(p), price(pr) {}

	uint32_t get_price() const override {
		return price;
	}

	std::string get_description() const override {
		return "Book: title - " + title + " pages - " + std::to_string(pages) + " price - " + std::to_string(price) + "$";
	}
};


class Shirt : public Product {
private:
	uint32_t size;
	std::string picture;
	uint32_t price;

public:
	Shirt(uint32_t s, std::string pic, uint32_t pr) : picture(std::move(pic)), size(s), price(pr) {}

	uint32_t get_price() const override {
		return price;
	}

	std::string get_description() const override {
		return "Shirt: size - " + std::to_string(size) + " picture - " + picture + " price - " + std::to_string(price) + "$";
	}
};

class Shose : public Product {
private:
	uint32_t size;
	std::string picture;
	uint32_t price;

public:
	Shose(uint32_t s, std::string pic, uint32_t pr) : picture(std::move(pic)), size(s), price(pr) {}

	uint32_t get_price() const override {
		return price;
	}

	std::string get_description() const override {
		return "Shose: size - " + std::to_string(size) + " picture - " + picture + " price - " + std::to_string(price) + "$";
	}
};



class Package : public Product {
private:
	std::vector<std::shared_ptr<Product>> products;

public:
	Package(std::vector<std::shared_ptr<Product>> prods) : products(std::move(prods)) {}

	uint32_t get_price() const override {
		uint32_t sum = 0;
		for (const auto& prod : products) {
			sum += prod->get_price();
		}
		sum = sum >= 10 ? sum - 10 : 0;
		return sum;
	}

	std::string get_description() const override {
		std::string descript = "Gift Package containing:\n";
		for (const auto& prod : products) {
			descript += " " + prod->get_description() + "\n";
		}
		return descript;
	}

};


/*	---------------- Order: set up orders and orders related things -------------- */


class Order {
private:

	std::vector<std::shared_ptr<Product>> products;

public:

	Order(std::vector<std::shared_ptr<Product>> prods) 
		: products(std::move(prods)) {}

	uint32_t getPrice() const{
		uint32_t sum = 0;
		for (auto& p : products) {
			sum += p->get_price();
		}
		return sum;
	}

	void printOrder() const{
		for (auto& p : products) {
			std::cout << p->get_description() << "\n";
		}
		std::cout << "Total " << getPrice() << "$" << "\n";
	}
};


/* --------------- Store: Manage orders for customers ---------------- */

class Store {
private:

	std::unordered_map<uint32_t, std::vector<Order>> customers;

	std::vector<std::shared_ptr<Product>> products;

public:

	void addProductToStore(std::shared_ptr<Product> new_prod) {
		products.push_back(std::move(new_prod));
	}

	void putOrder(Order order, uint32_t c_id) {
		customers[c_id].push_back(std::move(order));
	}

	const std::vector<Order>& getCustomerOrders(uint32_t c_id) const{
		static const std::vector<Order> emptyList;
		auto it = customers.find(c_id);
		if (it != customers.end()) {
			return it->second;
		}

		return emptyList;
	}

	void createPackage(std::vector<std::shared_ptr<Product>> prods) {
		products.push_back(std::make_unique<Package>(std::move(prods)));
	}

	void PrintCatalog() {
		for (const auto& p : products) {
			std::cout << p->get_description() << "\n\n";
		}
	}
};

int main() {
	Store store;

	// 1. Create your "Catalog" of objects once using make_shared
	std::shared_ptr<Product> black_shirt = std::make_shared<Shirt>(18, "black_shirt.png", 100);
	std::shared_ptr<Product> white_shoes = std::make_shared<Shose>(42, "white_shoes.png", 200);
	std::shared_ptr<Product> cpp_book = std::make_shared<Book>("C++ Guide", 400, 150);
	std::shared_ptr<Product> cpp_book2 = std::make_shared<Book>("C++ Guide", 400, 150);

	// 2. Add them to the Store's catalog (if your store tracks available products)
	store.addProductToStore(black_shirt);
	store.addProductToStore(white_shoes);
	store.addProductToStore(cpp_book);
	store.addProductToStore(cpp_book2);

	// 3. Create Order 1
	std::vector<std::shared_ptr<Product>> cart1;
	cart1.push_back(black_shirt); // Adds the shared pointer
	cart1.push_back(cpp_book);
	Order order1(std::move(cart1));

	// 4. Create Order 2 (Reusing the EXACT SAME shirt!)
	std::vector<std::shared_ptr<Product>> cart2;
	cart2.push_back(black_shirt); // No typos, no duplicated memory!
	cart2.push_back(white_shoes);
	cart2.push_back(black_shirt); // Customer bought 2 of the same shirt!
	Order order2(std::move(cart2));

	// 5. Submit orders
	store.putOrder(std::move(order1), 1);
	store.putOrder(std::move(order2), 2);

	const auto& cast1 = store.getCustomerOrders(1);

	for (const auto& ord : cast1) {
		ord.printOrder();
	}
	std::cout << "\n";
	const auto& cast2 = store.getCustomerOrders(2);
	for (const auto& ord : cast2) {
		ord.printOrder();
	}

	std::vector<std::shared_ptr<Product>> packeg1 = { black_shirt, white_shoes};
	store.createPackage(packeg1);
	store.PrintCatalog();
	return 0;
}