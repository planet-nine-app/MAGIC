//
//  Network.swift
//  MAGICPOD
//
//  Created by Zach Babb on 6/15/24.
//

import Foundation
import MAGICPOD

//let baseURL = "https://thirsty-gnu-80.deno.dev/"
let baseURL = "http://127.0.0.1:3000/"

enum NetworkError: Error {
    case networkError
}

class Network {
    class func get(urlString: String, callback: @escaping (Error?, Data?) -> Void) async {
        guard let url = URL(string: urlString) else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "GET"
        request.setValue("application/json", forHTTPHeaderField: "Accept")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        do {
            let (data, response) = try await URLSession.shared.data(for: request)
            guard let httpResponse = response as? HTTPURLResponse else { return }
            if httpResponse.statusCode > 300 {
                callback(NetworkError.networkError, nil)
                return
            }
            callback(nil, data)
        } catch {
            callback(error, nil)
        }
    }
    
    class func post(urlString: String, payload: Data, callback: @escaping (Error?, Data?) -> Void) async {
        print(urlString)
        print("urlString ^^^")
        guard let url = URL(string: urlString) else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Accept")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        do {
            let (data, _) = try await URLSession.shared.upload(for: request, from: payload)
            callback(nil, data)
        } catch {
            callback(error, nil)
        }
    }
    
    class func put(urlString: String, payload: Data, callback: @escaping (Error?, Data?) -> Void) async {
        print(urlString)
        print("urlString ^^^")
        guard let url = URL(string: urlString) else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "PUT"
        request.setValue("application/json", forHTTPHeaderField: "Accept")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        do {
            let (data, _) = try await URLSession.shared.upload(for: request, from: payload)
            callback(nil, data)
        } catch {
            callback(error, nil)
        }
    }
    
    class func forwardSpell(payload: Spell, callback: @escaping (Error?, Data?) -> Void) async {
        guard let data = payload.toString().data(using: .utf8) else { return }
        await Network.post(urlString: "\(baseURL)resolve/spell/demo", payload: data) { err, data in
            if let err = err {
                print(err)
                print("errororororo")
            }
            callback(err, data)
        }
    }
}
